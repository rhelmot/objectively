use hashbrown::raw::RawTable;
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
    table: RawTable<GDictEntry<T>>
}

impl<T> GDict<T> {
    pub fn insert(&mut self, gil: &mut Gil, key: Object, value: T) -> NullResult {
        let hash = hash_inner(gil, key.clone())?;
        if let Some(cell) = self.raw_get_mut(gil, hash, key.clone())? {
            *cell = value;
        } else {
            // why no try_insert method :(
            if self.table.try_reserve(1, entry_hasher).is_err() {
                return Err(MEMORYERROR_INST.clone());
            }
            self.table.try_insert_no_grow(hash, GDictEntry { hash, key, value, });
        }

        Ok(())
    }

    pub fn get(&mut self, gil: &mut Gil, key: Object) -> Result<Option<&T>> {
        let hash = hash_inner(gil, key.clone())?;
        if let Some(cell) = self.raw_get_mut(gil, hash, key.clone())? {
            Ok(Some(cell))
        } else {
            Ok(None)
        }
    }

    pub fn pop(&mut self, gil: &mut Gil, key: Object) -> Result<Option<T>> {
        todo!()
    }

    fn raw_get_mut(&mut self, gil: &mut Gil, hash: u64, key: Object) -> Result<Option<&mut T>> {
        // safety: TODO skye pls help
        unsafe {
            for bucket in self.table.iter_hash(hash) {
                let elm = bucket.as_mut();
                if hash != elm.hash {
                    continue;
                }
                if eq_inner(gil, key.clone(), elm.key.clone())? {
                    return Ok(Some(&mut elm.value));
                }
            }
            Ok(None)
        }
    }
}