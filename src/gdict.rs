use std::marker::PhantomData;
use std::ops::{Deref, DerefMut};
use hashbrown::raw::{RawTable, RawIter, RawIterHash, Bucket};
use shredder::{Scan, Scanner};
use shredder::marker::{GcDrop, GcDeref, GcSafe};

use crate::object::{Object, Result, ExceptionObject, Gil, NullResult, MEMORYERROR_INST};
use crate::builtins::{eq_inner, hash_inner};

pub struct GDictEntry<T> {
    hash: u64,
    key: Object,
    value: T
}

fn entry_hasher<T>(e: &GDictEntry<T>) -> u64 {
    e.hash
}

pub struct GDict<T> {
    table: RawTable<GDictEntry<T>>,
    generation: u64,
}
pub trait HasDict<T>: Clone where T: Scan {
    type Deref<'a>: Deref<Target=GDict<T>> where Self: 'a;
    fn get_dict<'a>(&'a self, gil: &'a Gil) -> Self::Deref<'a>;
    type DerefMut<'a>: DerefMut<Target=GDict<T>> where Self: 'a;
    fn get_dict_mut<'a>(&'a self, gil: &'a mut Gil) -> Self::DerefMut<'a>;
}

pub struct GDictIterator<'a, T, F: 'a> {
    dict_fn: F,
    generation: u64,
    iter: RawIter<GDictEntry<T>>,
    _phantom: PhantomData<&'a F>,
}
pub struct GDictMutIterator<'a, T, F: 'a> {
    dict_fn: F,
    generation: u64,
    iter: RawIter<GDictEntry<T>>,
    current_bucket: Option<Bucket<GDictEntry<T>>>,
    _phantom: PhantomData<&'a F>,
}
pub struct GDictHashIterator<'a, T, F> {
    dict_fn: F,
    generation: u64,
    hash: u64,
    iter: RawIterHash<'a, GDictEntry<T>>,
}
pub struct GDictMutHashIterator<'a, T, F: 'a> {
    dict_fn: F,
    generation: u64,
    hash: u64,
    iter: RawIterHash<'a, GDictEntry<T>>,
    current_bucket: Option<Bucket<GDictEntry<T>>>,
}

impl<T> GDict<T> where T: Scan {
    pub fn new() -> GDict<T> {
        GDict {
            table: RawTable::new(),
            generation: 0,
        }
    }

    pub fn insert(gil: &mut Gil, this_fn: impl HasDict<T>, key: Object, value: T) -> NullResult {
        let hash = hash_inner(gil, key.clone())?;

        if let Some(cell) = GDict::raw_get_mut(gil, this_fn.clone(), key.clone(), hash)? {
            *cell = value;
        } else {
            // why no try_insert method :(
            let mut this = this_fn.get_dict_mut(gil);
            if this.table.try_reserve(1, entry_hasher).is_err() {
                return Err(MEMORYERROR_INST.clone());
            }
            this.generation += 1;
            this.table.try_insert_no_grow(hash, GDictEntry { hash, key, value, }).unwrap_or_else(|_| unreachable!());
        }

        Ok(())
    }

    pub fn get<'a>(gil: &'a mut Gil, this_fn: impl HasDict<T> + 'a, key: Object) -> Result<Option<&'a T>> {
        let hash = hash_inner(gil, key.clone())?;
        GDict::raw_get(gil, this_fn, key.clone(), hash)
    }

    pub fn pop(gil: &mut Gil, this_fn: impl HasDict<T>, key: Object) -> Result<Option<T>> {
        let hash = hash_inner(gil, key.clone())?;
        let mut iter = GDictMutHashIterator::new(gil, this_fn, hash);
        loop {
            if let Some(entry) = iter.next(gil)? {
                if eq_inner(gil, &key, entry.key.clone())? {
                    return Ok(Some(iter.pop(gil)?));
                }
            } else {
                return Ok(None);
            }
        }
    }

    fn raw_get_mut<'a>(gil: &'a mut Gil, this_fn: impl HasDict<T> + 'a, key: Object, hash: u64) -> Result<Option<&'a mut T>> {
        let mut iter = GDictMutHashIterator::new(gil, this_fn, hash);
        loop {
            if let Some(entry) = iter.next(gil)? {
                if eq_inner(gil, &key, entry.key.clone())? {
                    return Ok(Some(&mut entry.value));
                }
            } else {
                return Ok(None);
            }
        }
    }

    fn raw_get<'a>(gil: &'a mut Gil, this_fn: impl HasDict<T> + 'a, key: Object, hash: u64) -> Result<Option<&'a T>> {
        let mut iter = GDictHashIterator::new(gil, this_fn, hash);
        loop {
            if let Some(entry) = iter.next(gil)? {
                if eq_inner(gil, &key, entry.key.clone())? {
                    return Ok(Some(&entry.value));
                }
            } else {
                return Ok(None);
            }
        }
    }
}

impl<'a, T, F> GDictIterator<'a, T, F>
where F: HasDict<T> + 'a, T: Scan {
    pub fn new(gil: &Gil, dict: F) -> Self {
        let theref = dict.get_dict(gil);
        GDictIterator {
            dict_fn: dict.clone(),
            generation: theref.generation,
            iter: unsafe { theref.table.iter() },
            _phantom: PhantomData
        }
    }

    pub fn next(&mut self, gil: &Gil) -> Result<Option<&GDictEntry<T>>> {
        let dict = self.dict_fn.get_dict(gil);
        if dict.generation != self.generation {
            return Err(ExceptionObject::runtime_error("Dict keys were modified during iteration")?);
        }

        match self.iter.next() {
            None => Ok(None),
            // safety: TODO what am I doiiiiiiiing
            Some(b) => unsafe { Ok(Some(&b.as_ref())) }
        }
    }
}

impl<'a, T, F> GDictMutIterator<'a, T, F>
where F: HasDict<T> + 'a, T: Scan {
    pub fn new(gil: &mut Gil, dict: F) -> Self {
        let theref = dict.get_dict_mut(gil);
        GDictMutIterator {
            dict_fn: dict.clone(),
            generation: theref.generation,
            iter: unsafe { theref.table.iter() },
            current_bucket: None,
            _phantom: PhantomData,
        }
    }

    pub fn next(&mut self, gil: &mut Gil) -> Result<Option<&mut GDictEntry<T>>> {
        let dict = self.dict_fn.get_dict_mut(gil);
        if dict.generation != self.generation {
            return Err(ExceptionObject::runtime_error("Dict keys were modified during iteration")?);
        }

        match self.iter.next() {
            None => Ok(None),
            // safety: TODO what am I doiiiiiiiing
            Some(b) => {
                self.current_bucket = Some(b.clone());
                unsafe { Ok(Some(b.as_mut())) }
            }
        }
    }

    pub fn pop(&mut self, gil: &mut Gil) -> Result<T> {
        let mut dict = self.dict_fn.get_dict_mut(gil);
        if dict.generation != self.generation {
            return Err(ExceptionObject::runtime_error("Dict keys were modified during iteration")?);
        }


        let mut bucket: Option<Bucket<GDictEntry<T>>> = None;
        std::mem::swap(&mut bucket, &mut self.current_bucket);
        dict.generation += 1;
        Ok(unsafe { dict.table.remove(bucket.expect("Misuse of GDict iterators: must call next before pop")).value })
    }
}

impl<'a, T, F> GDictHashIterator<'a, T, F>
    where F: HasDict<T> + 'a, T: Scan {
    pub fn new(gil: &Gil, dict: F, hash: u64) -> Self {
        let theref = dict.get_dict(gil);
        GDictHashIterator {
            dict_fn: dict.clone(),
            generation: theref.generation,
            hash,
            // safety: TODO FIXME XXX AAAAAAAAAAAAAAAAAAAAAAAAAA
            iter: unsafe { std::mem::transmute(theref.table.iter_hash(hash)) },
        }
    }

    pub fn next(&mut self, gil: &Gil) -> Result<Option<&'a GDictEntry<T>>> {
        let dict = self.dict_fn.get_dict(gil);
        if dict.generation != self.generation {
            return Err(ExceptionObject::runtime_error("Dict keys were modified during iteration")?);
        }

        loop {
            if let Some(b) = self.iter.next() {
                // safety: TODO what am I doiiiiiiiing
                unsafe {
                    if b.as_ref().hash == self.hash {
                        return Ok(Some(b.as_ref()));
                    }
                }
            } else {
                return Ok(None);
            }
        }
    }
}

impl<'a, T, F> GDictMutHashIterator<'a, T, F>
    where F: HasDict<T> + 'a, T: Scan {
    pub fn new(gil: &mut Gil, dict: F, hash: u64) -> Self {
        let theref = dict.get_dict_mut(gil);
        GDictMutHashIterator {
            dict_fn: dict.clone(),
            generation: theref.generation,
            hash,
            // safety: TODO FIXME XXX AAAAAAAAAAAAAAAAAAAAAAAAAA
            iter: unsafe { std::mem::transmute(theref.table.iter_hash(hash)) },
            current_bucket: None,
        }
    }

    pub fn next(&mut self, gil: &mut Gil) -> Result<Option<&'a mut GDictEntry<T>>> {
        let dict = self.dict_fn.get_dict_mut(gil);
        if dict.generation != self.generation {
            return Err(ExceptionObject::runtime_error("Dict keys were modified during iteration")?);
        }

        loop {
            if let Some(b) = self.iter.next() {
                // safety: TODO what am I doiiiiiiiing
                unsafe {
                    if b.as_ref().hash == self.hash {
                        self.current_bucket = Some(b.clone());
                        return Ok(Some(b.as_mut()));
                    }
                }
            } else {
                return Ok(None);
            }
        }
    }

    pub fn pop(&mut self, gil: &mut Gil) -> Result<T> {
        let mut dict = self.dict_fn.get_dict_mut(gil);
        if dict.generation != self.generation {
            return Err(ExceptionObject::runtime_error("Dict keys were modified during iteration")?);
        }


        let mut bucket: Option<Bucket<GDictEntry<T>>> = None;
        std::mem::swap(&mut bucket, &mut self.current_bucket);
        dict.generation += 1;
        Ok(unsafe { dict.table.remove(bucket.expect("Misuse of GDict iterators: must call next before pop")).value })
    }
}

unsafe impl<T> Scan for GDict<T>
where T: Scan,
    {
    fn scan(&self, scanner: &mut Scanner<'_>) {
        unsafe {
            for entry in self.table.iter() {
                scanner.scan(&entry.as_ref().key);
                scanner.scan(&entry.as_ref().value);
            }
        }
    }
}

// TODO uhhhhhhhhhhhh what do these actually do
unsafe impl<T> GcDeref for GDict<T> where T: GcDeref {}
unsafe impl<T> GcDrop for GDict<T> where T: GcDrop {}
unsafe impl<T> GcSafe for GDict<T> where T: GcSafe {}
