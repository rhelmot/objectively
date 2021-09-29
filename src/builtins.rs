use crate::object::{Object, ObjectResult, TupleObject, IntObject, Exception, ExceptionKind, ObjectTrait, VecResult, GenericResult};
use crate::gcell::GCellOwner;

pub fn obj_iter_collect(mut gil: GCellOwner, iterable: Object) -> VecResult {
    let mut items = vec![];
    let (mut gil, mut iter_method) = iterable.get().ro(&gil).get_attr(gil, "__iter__")?;
    let (mut gil, mut iter) = iter_method.get().rw(&mut gil).call(gil, TupleObject::new0())?;
    loop {
        let (mut gil, mut next_method) = iter.get().ro(&gil).get_attr(gil, "__next__")?;
        let GenericResult(mut gil, mut next) = next_method.get().ro(&gil).call(gil, TupleObject::new0());
        match next {
            Ok(obj) => {
                items.push(obj);
            },
            Err(e) => {
                if e.kind == ExceptionKind::StopIteration {
                    break;
                } else {
                    return GenericResult(gil, Err(e));
                }
            }
        }
    }
    GenericResult(gil, Ok(items))
}

pub fn type_constructor(mut gil: GCellOwner, this: Object, args: TupleObject) -> ObjectResult {
    todo!()
}

pub fn object_constructor(mut gil: GCellOwner, thi: Object, args: TupleObject) -> ObjectResult {
    todo!()
}

pub fn tuple_constructor(mut gil: GCellOwner, this: Object, args: TupleObject) -> ObjectResult {
    if args.data.len() == 0 {
        GenericResult(gil, Ok(TupleObject::new0().into_gc()))
    } else if args.data.len() == 1 {
        let (mut gil, res) = obj_iter_collect(gil, args.data[0].clone())?;
        GenericResult(gil, Ok(TupleObject::new(res).into_gc()))
    } else {
        GenericResult(gil, Err(Exception::type_error("expected 0 or 1 arguments")))
    }
}

pub fn int_constructor(mut gil: GCellOwner, this: Object, args: TupleObject) -> ObjectResult {
    let result = if args.data.len() == 0 {
        GenericResult(gil, Ok(IntObject::new(0).into_gc()))
    } else if args.data.len() == 1 {
        let (mut gil, mut int_method) = args.data[0].get().ro(&gil).get_attr(gil, "__int__")?;
        int_method.get().ro(&gil).call(gil, TupleObject::new1(IntObject::new(10).into_gc()))
    } else if args.data.len() == 2 {
        if args.data[1].get().ro(&gil).as_int().is_none() {
            return GenericResult(gil, Err(Exception::type_error("base parameter must be int")))
        }
        let (mut gil, mut int_method) = args.data[0].get().ro(&gil).get_attr(gil, "__int__")?;
        int_method.get().ro(&gil).call(gil, TupleObject::new1(args.data[1].clone()))
    } else {
        return GenericResult(gil, Err(Exception::type_error("expected 0-2 arguments")))
    };

    if result.1.is_ok() && result.1.unwrap().get().ro(&gil).as_int().is_none() {
        return GenericResult(gil, Err(Exception::type_error("__int__ did not return an int")));
    }
    result
}