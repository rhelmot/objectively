use hashbrown::raw::{RawTable, RawIter, RawIterHash, Bucket};
use shredder::{Scan, Scanner};
use shredder::marker::{GcDrop, GcDeref, GcSafe};
use crate::gcell::GCellOwner;

use crate::object::{G, Object, TupleObject, Result, ObjectResult, ExceptionObject, GcGCellExt, Gil, NullResult, MEMORYERROR_INST};
use crate::builtins::{eq_inner, hash_inner};

struct GDictEntry<T> {
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
pub trait HasDict<T>: Clone {
    fn get_dict<'a>(&'a self, gil: &'a Gil) -> &'a GDict<T>;
    fn get_dict_mut<'a>(&'a self, gil: &'a mut Gil) -> &'a mut GDict<T>;
}

pub struct GDictIterator<T, F> {
    dict_fn: F,
    generation: u64,
    iter: RawIter<GDictEntry<T>>,
}
pub struct GDictMutIterator<T, F> {
    dict_fn: F,
    generation: u64,
    iter: RawIter<GDictEntry<T>>,
    current_bucket: Option<Bucket<GDictEntry<T>>>,
}
pub struct GDictHashIterator<T, F> {
    dict_fn: F,
    generation: u64,
    hash: u64,
    iter: RawIterHash<'static, GDictEntry<T>>,
}
pub struct GDictMutHashIterator<T, F> {
    dict_fn: F,
    generation: u64,
    hash: u64,
    iter: RawIterHash<'static, GDictEntry<T>>,
    current_bucket: Option<Bucket<GDictEntry<T>>>,
}

impl<T> GDict<T> {
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
            let this = this_fn.get_dict_mut(gil);
            if this.table.try_reserve(1, entry_hasher).is_err() {
                return Err(MEMORYERROR_INST.clone());
            }
            this.generation += 1;
            this.table.try_insert_no_grow(hash, GDictEntry { hash, key, value, });
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
                if eq_inner(gil, key.clone(), entry.key.clone())? {
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
                if eq_inner(gil, key.clone(), entry.key.clone())? {
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
            let next = iter.next(gil)?.clone();
            if let Some(entry) = next {
                if eq_inner(gil, key.clone(), entry.key.clone())? {
                    return Ok(Some(&entry.value));
                }
            } else {
                return Ok(None);
            }
        }
    }
}

impl<T, F> GDictIterator<T, F>
where F: HasDict<T> {
    pub fn new(gil: &Gil, mut dict: F) -> GDictIterator<T, F> {
        let theref = dict.get_dict(gil);
        GDictIterator {
            dict_fn: dict.clone(),
            generation: theref.generation,
            iter: unsafe { theref.table.iter() },
        }
    }

    pub fn next(&mut self, gil: &Gil) -> Result<Option<&GDictEntry<T>>> {
        let dict: &GDict<T> = self.dict_fn.get_dict(gil);
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

impl<T, F> GDictMutIterator<T, F>
where F: HasDict<T> {
    pub fn new(gil: &mut Gil, mut dict: F) -> GDictMutIterator<T, F> {
        let theref = dict.get_dict_mut(gil);
        GDictMutIterator {
            dict_fn: dict.clone(),
            generation: theref.generation,
            iter: unsafe { theref.table.iter() },
            current_bucket: None,
        }
    }

    pub fn next(&mut self, gil: &mut Gil) -> Result<Option<&mut GDictEntry<T>>> {
        let dict: &mut GDict<T> = self.dict_fn.get_dict_mut(gil);
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
        let dict: &mut GDict<T> = self.dict_fn.get_dict_mut(gil);
        if dict.generation != self.generation {
            return Err(ExceptionObject::runtime_error("Dict keys were modified during iteration")?);
        }


        let mut bucket: Option<Bucket<GDictEntry<T>>> = None;
        std::mem::swap(&mut bucket, &mut self.current_bucket);
        dict.generation += 1;
        Ok(unsafe { dict.table.remove(bucket.expect("Misuse of GDict iterators: must call next before pop")).value })
    }
}

impl<T, F> GDictHashIterator<T, F>
    where F: HasDict<T> {
    pub fn new(gil: &Gil, mut dict: F, hash: u64) -> GDictHashIterator<T, F> {
        let theref = dict.get_dict(gil);
        GDictHashIterator {
            dict_fn: dict.clone(),
            generation: theref.generation,
            hash,
            // safety: TODO FIXME XXX AAAAAAAAAAAAAAAAAAAAAAAAAA
            iter: unsafe { std::mem::transmute(theref.table.iter_hash(hash)) },
        }
    }

    pub fn next(&mut self, gil: &Gil) -> Result<Option<&GDictEntry<T>>> {
        let dict: &GDict<T> = self.dict_fn.get_dict(gil);
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

impl<T, F> GDictMutHashIterator<T, F>
    where F: HasDict<T> {
    pub fn new(gil: &mut Gil, dict: F, hash: u64) -> GDictMutHashIterator<T, F> {
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

    pub fn next(&mut self, gil: &mut Gil) -> Result<Option<&mut GDictEntry<T>>> {
        let dict: &mut GDict<T> = self.dict_fn.get_dict_mut(gil);
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
        let dict: &mut GDict<T> = self.dict_fn.get_dict_mut(gil);
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
