use hashbrown::raw::RawTable;
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

impl<T> GDict<T> {
    pub fn new() -> GDict<T> {
        GDict {
            table: RawTable::new(),
            generation: 0,
        }
    }

    pub fn insert(&mut self, gil: &mut Gil, key: Object, value: T) -> NullResult {
        let hash = hash_inner(gil, key.clone())?;
        if let Some(cell) = self.raw_get_mut(gil, hash, key.clone())? {
            *cell = value;
        } else {
            // why no try_insert method :(
            if self.table.try_reserve(1, entry_hasher).is_err() {
                return Err(MEMORYERROR_INST.clone());
            }
            self.generation += 1;
            self.table.try_insert_no_grow(hash, GDictEntry { hash, key, value, });
        }

        Ok(())
    }

    pub fn get(&mut self, gil: &mut Gil, key: Object) -> Result<Option<&T>> {
        let hash = hash_inner(gil, key.clone())?;
        // TODO stop being lazy; don't require a mutable reference
        if let Some(cell) = self.raw_get_mut(gil, hash, key.clone())? {
            Ok(Some(cell))
        } else {
            Ok(None)
        }
    }

    pub fn pop(&mut self, gil: &mut Gil, key: Object) -> Result<Option<T>> {
        let hash = hash_inner(gil, key.clone())?;
        // safety: TODO skye pls help
        unsafe {
            for bucket in self.table.iter_hash(hash) {
                let elm = bucket.as_mut();
                if hash != elm.hash {
                    continue;
                }
                // TODO: will the compiler optimize this as if we're following aliasing rules?
                let gen = self.generation;
                let eq = eq_inner(gil, key.clone(), elm.key.clone())?;
                if gen != self.generation {
                    return Err(ExceptionObject::runtime_error("Dict was mutated during iteration")?);
                }
                if eq {
                    self.generation += 1;
                    return Ok(Some(self.table.remove(bucket).value));
                }
            }
            Ok(None)
        }
    }

    fn raw_get_mut(&mut self, gil: &mut Gil, hash: u64, key: Object) -> Result<Option<&mut T>> {
        // safety: as above
        unsafe {
            for bucket in self.table.iter_hash(hash) {
                let elm = bucket.as_mut();
                if hash != elm.hash {
                    continue;
                }
                // TODO: as above
                let gen = self.generation;
                let eq = eq_inner(gil, key.clone(), elm.key.clone())?;
                if gen != self.generation {
                    return Err(ExceptionObject::runtime_error("Dict was mutated during iteration")?)
                }
                if eq {
                    return Ok(Some(&mut elm.value));
                }
            }
            Ok(None)
        }
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
