use std::{convert::TryInto, ops::Deref, thread, time};
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

use parking_lot::MutexGuard;

use crate::{
    gcell::GCellOwner,
    object::{
        yield_gil, ExceptionObject, FloatObject, GcGCellExt, IntObject, Object, ObjectResult,
        ObjectTrait, TupleObject, TypeObject, VecResult, G_FALSE, G_NONE, G_STOPITERATION, Gil,
        Result, G, bool_raw, DictObject
    },
};
use crate::gdict::GDict;

fn raw_is(obj1: &Object, obj2: &Object) -> bool {
    obj1.raw_id() == obj2.raw_id()
}

pub fn obj_iter_collect(gil: &mut MutexGuard<GCellOwner>, iterable: &Object) -> VecResult {
    let mut items = vec![];
    let iter: Object = iterable.call_method("__iter__", gil, TupleObject::new0()?)?;
    loop {
        let next = iter.call_method("__next__", gil, TupleObject::new0()?);
        match next {
            Ok(obj) => {
                items.push(obj);
            }
            Err(e) => {
                return if e.get_type(gil).is(G_STOPITERATION.deref()) {
                    Ok(items)
                } else {
                    Err(e)
                }
            }
        }
    }
}

pub fn type_constructor(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    match args.data.as_slice() {
        [] => todo!(),
        [obj] => Ok(obj.get_type(gil).into()),
        [subty_, members_] => {
            if let (Object::Type(subty), Object::Dict(members)) = (subty_, members_) {
                Ok(TypeObject {
                    ty: todo!(),
                    base_class: Some(subty.clone()),
                    constructor: subty.get().ro(gil).constructor,
                }
                .into_object())
            } else {
                Err(todo!())
            }
        }
        [_, _, _, ..] => todo!(),
    }
}

pub fn exc_constructor(
    _gil: &mut MutexGuard<GCellOwner>,
    this: Object,
    args: TupleObject,
) -> ObjectResult {
    Ok(ExceptionObject {
        ty: this.try_into().unwrap(),
        args: args.into_gc(),
    }
    .into_object())
}

pub fn object_constructor(
    _gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    _args: TupleObject,
) -> ObjectResult {
    todo!()
}

pub fn null_constructor(
    _gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    _args: TupleObject,
) -> ObjectResult {
    Err(ExceptionObject::type_error("Cannot construct this object")?)
}

pub fn tuple_constructor(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    match args.data.as_slice() {
        [] => Ok(TupleObject::new0()?.into()),
        [arg] => {
            let res = obj_iter_collect(gil, arg)?;
            Ok(TupleObject::new(res)?.into())
        }
        _ => Err(ExceptionObject::type_error("expected 0 or 1 arguments")?),
    }
}

pub fn int_constructor(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    let result = match args.data.as_slice() {
        [] => IntObject::new(0)?.into(),
        [arg] => arg.call_method(
            "__int__",
            gil,
            TupleObject::new1(IntObject::new(0)?.into())?,
        )?,
        [arg, base] => {
            if let Object::Int(_) = base {
            } else {
                return Err(ExceptionObject::type_error("base parameter must be int")?);
            }
            arg.call_method("__int__", gil, TupleObject::new1(base.clone())?)?
        }
        _ => return Err(ExceptionObject::type_error("expected 0-2 arguments")?),
    };

    if let Object::Int(_) = result {
        Ok(result)
    } else {
        Err(ExceptionObject::type_error(
            "__int__ did not return an int",
        )?)
    }
}

pub fn float_constructor(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    let result = match args.data.as_slice() {
        [] => FloatObject::new(0.0)?.into(),
        [arg] => arg.call_method(
            "__float__",
            gil,
            TupleObject::new1(FloatObject::new(0.0)?.into())?,
        )?,
        [arg, base] => {
            if let Object::Float(_) = base {
            } else {
                return Err(ExceptionObject::type_error("base parameter must be float")?);
            }
            arg.call_method("__float__", gil, TupleObject::new1(base.clone())?)?
        }
        _ => return Err(ExceptionObject::type_error("expected 0-2 arguments")?),
    };

    if let Object::Float(_) = result {
        Ok(result)
    } else {
        Err(ExceptionObject::type_error(
            "__float__ did not return an float",
        )?)
    }
}

pub fn nonetype_constructor(
    _gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    _args: TupleObject,
) -> ObjectResult {
    Ok(G_NONE.clone().into())
}

pub fn bytes_constructor(
    _gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    _args: TupleObject,
) -> ObjectResult {
    todo!()
}

pub fn dict_constructor(
    _gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    _args: TupleObject,
) -> ObjectResult {
    todo!()
}

pub fn bool_constructor(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    match args.data.as_slice() {
        [] => Ok(G_FALSE.clone().into()),
        [arg] => {
            let result = arg.call_method("__bool__", gil, TupleObject::new0()?);
            match result {
                Ok(Object::Bool(bool)) => Ok(Object::Bool(bool)),
                Ok(_) => Err(ExceptionObject::type_error(
                    "__bool__ did not return a bool",
                )?),
                Err(err) => Err(err),
            }
        }
        _ => Err(ExceptionObject::type_error("expected 0 or 1 args")?),
    }
}

pub fn id(_this: &Object, args: &TupleObject) -> ObjectResult {
    match args.data.as_slice() {
        [arg] => Ok(IntObject::new(arg.raw_id() as i64)?.into()),
        _ => Err(ExceptionObject::type_error("expected one argument")?),
    }
}

pub fn sleep(gil: &mut MutexGuard<GCellOwner>, _this: &Object, args: &TupleObject) -> ObjectResult {
    let duration = match args.data.as_slice() {
        [arg] => {
            match arg {
                Object::Int(i) => {
                    let seconds = i.get().ro(gil).data;
                    match seconds.try_into() {
                        Ok(seconds) => time::Duration::from_secs(seconds),
                        Err(_ /*std::num::TryFromIntError*/) => {
                            return Err(ExceptionObject::overflow_error(
                                "duration must be positive",
                            )?)
                        }
                    }
                }
                Object::Float(f) => {
                    let float = f.get().ro(gil).value;
                    match float {
                        _ if float.is_infinite() => {
                            return Err(ExceptionObject::overflow_error("duration must be finite")?)
                        }
                        _ if float.is_nan() => {
                            return Err(ExceptionObject::overflow_error("duration cannot be NaN")?)
                        }
                        _ if float.is_sign_negative() => {
                            return Err(ExceptionObject::overflow_error(
                                "duration must be non-negative",
                            )?)
                        }
                        _ => time::Duration::from_secs_f64(float),
                    }
                }

                _ => return Err(ExceptionObject::type_error("expected a number")?),
            }
        }
        _ => return Err(ExceptionObject::type_error("expected one argument")?),
    };
    yield_gil(gil, || {
        thread::sleep(duration);
    });
    Ok(G_NONE.clone().into())
}
pub fn hash_inner(gil: &mut Gil, key: Object) -> Result<u64>  {
    let h: Object = key.call_method("__hash__", gil, TupleObject::new0()?)?;
    if let Object::Int(i) = h {
        Ok(i.get().ro(gil).data as u64)
    } else {
        Err(ExceptionObject::type_error("__hash__ must return int")?)
    }
}

pub fn hash(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    if let [arg] = args.data.as_slice() {
        Ok(IntObject::new(hash_inner(gil, arg.clone())? as i64)?.into())
    } else {
        Err(ExceptionObject::type_error("Expected 1 argument")?)
    }
}

pub fn eq_inner(gil: &mut Gil, one: &Object, two: Object) -> Result<bool> {
    let h: Object = one.call_method("__eq__", gil, TupleObject::new1(two)?)?;
    if let Object::Bool(b) = h {
        Ok(bool_raw(b))
    } else {
        Err(ExceptionObject::type_error("__eq__ must return bool")?)
    }
}

pub fn dict_getitem(gil: &mut Gil, _this: Object, args: TupleObject) -> ObjectResult {
    match args.data.as_slice() {
        [Object::Dict(self_), key] => {
            match dict_getitem_inner(gil, self_, key.clone())? {
                None => Err(ExceptionObject::key_error(key.clone())?),
                Some(val) => Ok(val),
            }
        },
        _ => {
            Err(ExceptionObject::type_error("Excepted two arguments: (dict, *)")?)
        }
    }
}

pub fn dict_getitem_inner(gil: &mut Gil, dict: &G<DictObject>, key: Object) -> Result<Option<Object>> {
    if let Some(v) = GDict::get(gil, dict, key)? {
        Ok(Some(v.clone()))
    } else {
        Ok(None)
    }
}

pub fn int_hash(_gil: &mut Gil, _this: Object, args: TupleObject) -> ObjectResult {
    match args.data.as_slice() {
        [Object::Int(_)] => {
            Ok(args.data[0].clone())
        },
        _ => {
            Err(ExceptionObject::type_error("Expected one argument: int")?)
        }
    }
}

pub fn bytes_hash(gil: &mut Gil, _this: Object, args: TupleObject) -> ObjectResult {
    match args.data.as_slice() {
        [Object::Bytes(g_bytes)] => {
            let unwrapped = g_bytes.get().ro(gil);
            let mut hasher = DefaultHasher::new();
            unwrapped.value.hash(&mut hasher);
            Ok(IntObject::new(hasher.finish() as i64)?.into())
        },
        _ => {
            Err(ExceptionObject::type_error("Expected one argument: bytes")?)
        }
    }
}
