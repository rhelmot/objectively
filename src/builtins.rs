use std::{convert::TryInto, thread, time};

use parking_lot::MutexGuard;

use crate::{
    gcell::GCellOwner,
    object::{
        yield_gil, Exception, ExceptionKind, FloatObject, GcGCellExt, IntObject, Object,
        ObjectResult, ObjectTrait, TupleObject, VecResult, G_FALSE, G_NONE,
    },
};

fn raw_is(obj1: &Object, obj2: &Object) -> bool {
    obj1.raw_id() == obj2.raw_id()
}

pub fn obj_iter_collect(gil: &mut MutexGuard<GCellOwner>, iterable: &Object) -> VecResult {
    let mut items = vec![];
    let iter_method = iterable.get_attr(gil, "__iter__")?;
    let iter = iter_method.call(gil, TupleObject::new0())?;
    loop {
        let next_method = iter.get_attr(gil, "__next__")?;
        let next = next_method.call(gil, TupleObject::new0());
        match next {
            Ok(obj) => {
                items.push(obj);
            }
            Err(e) => {
                return if e.kind == ExceptionKind::StopIteration {
                    Ok(items)
                } else {
                    Err(e)
                }
            }
        }
    }
}

pub fn type_constructor(
    _gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    _args: TupleObject,
) -> ObjectResult {
    todo!()
}

pub fn object_constructor(
    _gil: &mut MutexGuard<GCellOwner>,
    _thi: Object,
    _args: TupleObject,
) -> ObjectResult {
    todo!()
}

pub fn tuple_constructor(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    match args.data.as_slice() {
        [] => Ok(TupleObject::new0().into_gc()),
        [arg] => {
            let res = obj_iter_collect(gil, arg)?;
            Ok(TupleObject::new(res).into_gc())
        }
        _ => Err(Exception::type_error("expected 0 or 1 arguments")),
    }
}

pub fn int_constructor(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    let result = match args.data.as_slice() {
        [] => IntObject::new(0).into_gc(),
        [arg] => arg
            .get_attr(gil, "__int__")?
            .call(gil, TupleObject::new1(IntObject::new(0).into_gc()))?,
        [arg, base] => {
            if let Object::Int(_) = base {
            } else {
                return Err(Exception::type_error("base parameter must be int"));
            }
            let int_method = arg.get_attr(gil, "__int__")?;
            int_method.call(gil, TupleObject::new1(base.clone()))?
        }
        _ => return Err(Exception::type_error("expected 0-2 arguments")),
    };

    if let Object::Int(_) = result {
        Ok(result)
    } else {
        Err(Exception::type_error("__int__ did not return an int"))
    }
}

pub fn float_constructor(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    let result = match args.data.as_slice() {
        [] => FloatObject::new(0.0).into_gc(),
        [arg] => arg
            .get_attr(gil, "__float__")?
            .call(gil, TupleObject::new1(FloatObject::new(0.0).into_gc()))?,
        [arg, base] => {
            if let Object::Float(_) = base {
            } else {
                return Err(Exception::type_error("base parameter must be float"));
            }
            let float_method = arg.get_attr(gil, "__float__")?;
            float_method.call(gil, TupleObject::new1(base.clone()))?
        }
        _ => return Err(Exception::type_error("expected 0-2 arguments")),
    };

    if let Object::Float(_) = result {
        Ok(result)
    } else {
        Err(Exception::type_error("__float__ did not return an float"))
    }
}

pub fn nonetype_constructor(
    _gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    _args: TupleObject,
) -> ObjectResult {
    Ok(G_NONE.clone())
}

pub fn bool_constructor(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    match args.data.as_slice() {
        [] => Ok(G_FALSE.clone()),
        [arg] => {
            let result = arg
                .get_attr(gil, "__bool__")?
                .call(gil, TupleObject::new0());
            match result {
                Ok(Object::Bool(bool)) => Ok(Object::Bool(bool)),
                Ok(_) => Err(Exception::type_error("__bool__ did not return a bool")),
                Err(err) => Err(err),
            }
        }
        _ => Err(Exception::type_error("expected 0 or 1 args")),
    }
}

pub fn id(_this: &Object, args: &TupleObject) -> ObjectResult {
    match args.data.as_slice() {
        [arg] => Ok(IntObject::new(arg.raw_id() as i64).into_gc()),
        _ => Err(Exception::type_error("expected one argument")),
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
                            return Err(Exception::overflow_error("duration must be positive"))
                        }
                    }
                }
                Object::Float(f) => {
                    let float = f.get().ro(gil).value;
                    match float {
                        _ if float.is_infinite() => {
                            return Err(Exception::overflow_error("duration must be finite"))
                        }
                        _ if float.is_nan() => {
                            return Err(Exception::overflow_error("duration cannot be NaN"))
                        }
                        _ if float.is_sign_negative() => {
                            return Err(Exception::overflow_error("duration must be non-negative"))
                        }
                        _ => time::Duration::from_secs_f64(float),
                    }
                }

                _ => return Err(Exception::type_error("expected a number")),
            }
        }
        _ => return Err(Exception::type_error("expected one argument")),
    };
    yield_gil(gil, || {
        thread::sleep(duration);
    });
    Ok(G_NONE.clone())
}
