use crate::builtins;
use crate::gcell::{GCell, GCellOwner};
use lazy_static::lazy_static;
use shredder::marker::GcDrop;
use shredder::{Gc, Scan, ToScan};
use std::collections::HashMap;
use std::convert::{TryInto};
use std::vec::Vec;

use parking_lot::{Mutex, MutexGuard};

pub type Object = Gc<GCell<dyn ObjectTrait>>;
pub type GenericResult<T> = Result<T, Exception>;
pub type NullResult = GenericResult<()>;
pub type ObjectResult = GenericResult<Object>;
pub type VecResult = GenericResult<Vec<Object>>;
pub type ObjectSelfFunction = fn(&mut MutexGuard<GCellOwner>, Object, TupleObject) -> ObjectResult;

#[derive(PartialEq, Eq, Debug)]
pub enum ExceptionKind {
    AttributeError,
    IndexError,
    TypeError,
    StopIteration,
    OverflowError,
}

#[derive(Debug)]
pub struct Exception {
    pub kind: ExceptionKind,
    pub msg: String,
    pub args: Vec<Object>,
}

impl Exception {
    pub fn attribute_error(name: &str) -> Exception {
        Exception {
            kind: ExceptionKind::AttributeError,
            msg: format!("No such attribute: {}", name),
            args: vec![], // TODO make an object out of name
        }
    }

    pub fn type_error(explanation: &str) -> Exception {
        Exception {
            kind: ExceptionKind::TypeError,
            msg: explanation.to_string(),
            args: vec![],
        }
    }

    pub fn overflow_error(explanation: &str) -> Exception {
        Exception {
            kind: ExceptionKind::OverflowError,
            msg: explanation.to_string(),
            args: vec![],
        }
    }
}

pub trait ObjectTrait: GcDrop + Scan + ToScan + Send + Sync {
    fn get_attr(&self, name: &str) -> ObjectResult {
        Err(Exception::attribute_error(name))
    }

    fn set_attr(&mut self, name: &str, _value: Object) -> NullResult {
        Err(Exception::attribute_error(name))
    }

    fn del_attr(&mut self, name: &str) -> NullResult {
        Err(Exception::attribute_error(name))
    }

    fn call(&mut self, gil: &mut MutexGuard<GCellOwner>, _args: TupleObject) -> ObjectResult {
        Err(Exception::type_error("Cannot call"))
    }

    fn get_type(&self) -> Object;

    fn as_tuple(&self) -> Option<&TupleObject> {
        None
    }
    fn as_int(&self) -> Option<&IntObject> {
        None
    }
    fn as_bool(&self) -> Option<&BoolObject> {
        None
    }

    fn into_gc(self) -> Object
    where
        Self: Sized,
        Self: 'static,
    {
        Gc::new(GCell::new(self))
    }
}

#[derive(Scan)]
pub struct TypeObject {
    pub name: String,
    pub base_class: Object,
    pub members: HashMap<String, Object>,
    pub constructor: &'static ObjectSelfFunction,
}
impl ObjectTrait for TypeObject {
    fn get_type(&self) -> Object {
        G_TYPE.clone()
    }
}

#[derive(Scan)]
pub struct TupleObject {
    pub data: Vec<Object>,
}

impl ObjectTrait for TupleObject {
    fn get_attr(&self, name: &str) -> ObjectResult {
        if name == "len" {
            match self.data.len().try_into() {
                Ok(v) => {
                    let int = IntObject::new(v);
                    Ok(int.into_gc())
                },
                Err(_) => {
                    Err(Exception::overflow_error(
                        "could not fit tuple length into integer object",
                    ))
                }
            }
        } else {
            Err(Exception::attribute_error(name))
        }
    }

    fn get_type(&self) -> Object {
        G_TUPLE.clone()
    }

    fn as_tuple(&self) -> Option<&TupleObject> {
        Some(self)
    }
}
impl TupleObject {
    pub fn new(args: Vec<Object>) -> TupleObject {
        TupleObject { data: args }
    }

    pub fn new0() -> TupleObject {
        TupleObject { data: vec![] }
    }

    pub fn new1(arg0: Object) -> TupleObject {
        TupleObject { data: vec![arg0] }
    }

    pub fn new2(arg0: Object, arg1: Object) -> TupleObject {
        TupleObject {
            data: vec![arg0, arg1],
        }
    }
}

#[derive(Scan)]
pub struct IntObject {
    pub data: i64,
}
impl ObjectTrait for IntObject {
    fn get_type(&self) -> Object {
        G_INT.clone()
    }

    fn as_int(&self) -> Option<&IntObject> {
        Some(self)
    }
}
impl IntObject {
    pub fn new(data: i64) -> IntObject {
        IntObject { data }
    }
}

pub struct BasicObject {
    pub members: HashMap<String, Object>,
    pub type_: Object,
}

#[derive(Scan)]
pub struct NoneObject{}
impl ObjectTrait for NoneObject {
    fn get_type(&self) -> Object {
        G_NONETYPE.clone()
    }
}

#[derive(Scan)]
pub struct BoolObject{
    pub data: bool,
}
impl ObjectTrait for BoolObject {
    fn get_type(&self) -> Object {
        G_BOOL.clone()
    }

    fn as_bool(&self) -> Option<&BoolObject> {
        Some(self)
    }
}
impl BoolObject {
    pub fn from_bool(data: bool) -> Object {
        if data { G_TRUE.clone() } else { G_FALSE.clone() }
    }
}

pub fn yield_gil<F>(guard: &mut MutexGuard<GCellOwner>, func: F) where F: FnOnce() {
    take_mut::take(guard, |guard| {
        drop(guard);
        func();
        GIL.lock()
    })
}

lazy_static! {
    pub static ref GIL: Mutex<GCellOwner> = Mutex::new(GCellOwner::make());
    pub static ref OBJECT_TYPE: Object = {
        #[derive(Scan)]
        struct Dummy;
        impl ObjectTrait for Dummy {
            fn get_type(&self) -> Object {
                unimplemented!("If this is ever called something has gone horribly wrong")
            }
        }

        let ty = Gc::new(GCell::new(TypeObject {
            name: "Object".to_string(),
            base_class: Gc::new(GCell::new(Dummy)),
            members: HashMap::new(),
            constructor: &(builtins::object_constructor as ObjectSelfFunction),
        }));
        ty.get().rw(&mut GIL.lock()).base_class = ty.clone();
        ty
    };
    pub static ref G_TYPE: Object = Gc::new(GCell::new(TypeObject {
        name: "type".to_string(),
        base_class: OBJECT_TYPE.clone(),
        members: HashMap::new(),
        constructor: &(builtins::type_constructor as ObjectSelfFunction),
    }));
    pub static ref G_TUPLE: Object = Gc::new(GCell::new(TypeObject {
        name: "tuple".to_string(),
        base_class: OBJECT_TYPE.clone(),
        members: HashMap::new(),
        constructor: &(builtins::tuple_constructor as ObjectSelfFunction),
    }));
    pub static ref G_INT: Object = Gc::new(GCell::new(TypeObject {
        name: "int".to_string(),
        base_class: OBJECT_TYPE.clone(),
        members: HashMap::new(),
        constructor: &(builtins::int_constructor as ObjectSelfFunction),
    }));
    pub static ref G_NONETYPE: Object = Gc::new(GCell::new(TypeObject {
        name: "nonetype".to_string(),
        base_class: OBJECT_TYPE.clone(),
        members: HashMap::new(),
        constructor: &(builtins::nonetype_constructor as ObjectSelfFunction),
    }));
    pub static ref G_BOOL: Object = Gc::new(GCell::new(TypeObject {
        name: "bool".to_string(),
        base_class: OBJECT_TYPE.clone(),
        members: HashMap::new(),
        constructor: &(builtins::bool_constructor as ObjectSelfFunction),
    }));
    pub static ref G_NONE: Object = Gc::new(GCell::new(NoneObject {}));
    pub static ref G_TRUE: Object = Gc::new(GCell::new(BoolObject { data: true }));
    pub static ref G_FALSE: Object = Gc::new(GCell::new(BoolObject { data: false }));
}
