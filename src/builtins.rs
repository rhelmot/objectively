use crate::gcell::GCellOwner;
use crate::object::{Exception, ExceptionKind, IntObject, Object, ObjectTrait, TupleObject,
                    VecResult, ObjectResult, yield_gil, G_NONE};
use std::{thread, time};
use parking_lot::MutexGuard;

pub fn obj_iter_collect(gil: &mut MutexGuard<GCellOwner>, iterable: Object) -> VecResult {
    let mut items = vec![];
    let iter_method = iterable.get().ro(gil).get_attr("__iter__")?;
    let iter = iter_method.get().rw(gil).call(TupleObject::new0())?;
    loop {
        let next_method = iter.get().ro(gil).get_attr("__next__")?;
        let next = next_method.get().rw(gil).call(TupleObject::new0());
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
            let int_method = arg.get().ro(gil).get_attr("__int__")?;
            int_method
                .get()
                .rw(gil)
                .call(TupleObject::new1(IntObject::new(10).into_gc()))
        }
        [arg, base] => {
            if base.get().ro(gil).as_int().is_none() {
                return Err(Exception::type_error("base parameter must be int"));
            }
            let int_method = arg.get().ro(gil).get_attr("__int__")?;
            int_method
                .get()
                .rw(gil)
                .call(TupleObject::new1(args.data[1].clone()))
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