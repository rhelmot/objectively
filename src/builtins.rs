use crate::gcell::GCellOwner;
use crate::object::{Exception, ExceptionKind, IntObject, Object, ObjectTrait, TupleObject,
                    BoolObject, VecResult, ObjectResult, yield_gil, G_NONE, G_FALSE, G_TRUE};
use std::{thread, time, ptr};
use parking_lot::MutexGuard;

fn raw_id(gil: &mut MutexGuard<GCellOwner>, obj: &Object) -> i64 {
    let ptr = obj.get().ro(&gil) as *const dyn ObjectTrait;
    ptr.to_raw_parts().0 as i64
}

fn raw_is(gil: &mut MutexGuard<GCellOwner>, obj1: &Object, obj2: &Object) -> bool {
    ptr::eq(obj1.get().ro(&gil), obj2.get().ro(&gil))
}

pub fn obj_iter_collect(gil: &mut MutexGuard<GCellOwner>, iterable: Object) -> VecResult {
    let mut items = vec![];
    let iter_method = iterable.get().ro(gil).get_attr("__iter__")?;
    let iter = iter_method.get().rw(gil).call(gil, TupleObject::new0())?;
    loop {
        let next_method = iter.get().ro(gil).get_attr("__next__")?;
        let next = next_method.get().rw(gil).call(gil, TupleObject::new0());
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
    _args: TupleObject
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
            let res = obj_iter_collect(gil, arg.clone())?;
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
        [] => Ok(IntObject::new(0).into_gc()),
        [arg] => {
            arg.get().ro(gil).get_attr("__int__")?
               .get().rw(gil).call(gil, TupleObject::new1(
                IntObject::new(10).into_gc()
            ))
        }
        [arg, base] => {
            if base.get().ro(gil).as_int().is_none() {
                return Err(Exception::type_error("base parameter must be int"));
            }
            let int_method = arg.get().ro(gil).get_attr("__int__")?;
            int_method
                .get()
                .rw(gil)
                .call(gil, TupleObject::new1(args.data[1].clone()))
        }
        _ => return Err(Exception::type_error("expected 0-2 arguments")),
    };

    if result.is_ok() && result.as_ref().unwrap().get().ro(gil).as_int().is_none() {
        return Err(Exception::type_error("__int__ did not return an int"));
    }
    result
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
            let result = arg.get().ro(&gil).get_attr("__bool__")?
                .get().rw(gil).call(gil, TupleObject::new0());
            if let Ok(maybe_a_bool) = result {
                if maybe_a_bool.get().ro(gil).as_bool().is_none() {
                    Err(Exception::type_error("__bool__ did not return a bool"))
                } else {
                    Ok(maybe_a_bool)
                }
            } else {
                result
            }
        }
        _ => Err(Exception::type_error("expected 0 or 1 args"))
    }
}

pub fn id(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    match args.data.as_slice() {
        [arg] => Ok(IntObject::new(raw_id(gil, arg)).into_gc()),
        _ => Err(Exception::type_error("expected one argument"))
    }
}

pub fn sleep(
    gil: &mut MutexGuard<GCellOwner>,
    _this: Object,
    args: TupleObject,
) -> ObjectResult {
    let duration = match args.data.as_slice() {
        [arg] => {
            if let Some(i) = arg.get().ro(&gil).as_int() {
                i.data as f64
            } else {
                return Err(Exception::type_error("expected int"))
                // TODO floats
            }
        }
        _ => return Err(Exception::type_error("expected one argument"))
    };
    yield_gil(gil, || {
        thread::sleep(time::Duration::from_secs_f64(duration))
    });
    Ok(G_NONE.clone())
}