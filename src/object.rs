use lazy_static::lazy_static;
use shredder::marker::GcDrop;
use shredder::{Gc, Scan, ToScan};
use std::collections::HashMap;
use std::sync::Mutex;
use std::vec::Vec;
use crate::gcell::{GCell, GCellOwner};

pub enum ExceptionKind {
    AttributeError,
    IndexError,
    TypeError,
}

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
            msg: format!("{}", explanation),
            args: vec![],
        }
    }
}

pub trait ObjectTrait: GcDrop + Scan + ToScan + Send + Sync {
    fn get_attr(&self, name: &str) -> Result<Object, Exception> {
        Err(Exception::attribute_error(name))
    }

    fn set_attr(&mut self, name: &str, value: Object) -> Result<(), Exception> {
        Err(Exception::attribute_error(name))
    }

    fn del_attr(&mut self, name: &str) -> Result<(), Exception> {
        Err(Exception::attribute_error(name))
    }

    fn call(&self, args: Object) -> Result<ThreadObject, Exception> {
        Err(Exception::type_error("Cannot call"))
    }

    fn get_type(&self) -> Object;
}
pub type Object = Gc<GCell<dyn ObjectTrait>>;

#[derive(Scan)]
pub struct TypeClassObject {
    pub name: String,
    pub base_class: Object,
    pub members: HashMap<String, Object>,
    pub constructor: &'static fn(Object, TupleObject) -> Result<Object, Exception>,
}
impl ObjectTrait for TypeClassObject {
    fn get_type(&self) -> Object {
        SINGLETON_TYPE.clone()
    }
}

#[derive(Scan)]
pub struct TupleObject {
    // Can we make TupleObject unsized somehow and have the data be directly part of the struct, like the C?
    pub data: Vec<Object>,
}
impl ObjectTrait for TupleObject {
    fn get_type(&self) -> Object {
        SINGLETON_TYPE.clone()
    }
}

#[derive(Scan)]
pub struct BuiltinClassObject {}
impl ObjectTrait for BuiltinClassObject {
    fn get_type(&self) -> Object {
        SINGLETON_TYPE.clone()
    }
}

pub struct ClosureObject {}
pub struct ThreadObject {}

pub struct BasicObject {
    pub members: HashMap<String, Object>,
    pub type_: Object,
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

        let ty = Gc::new(GCell::new(TypeClassObject {
            name: "Object".to_string(),
            base_class: Gc::new(GCell::new(Dummy)),
            members: HashMap::new(),
            constructor: &{ |_, _| Ok(OBJECT_TYPE.clone()) },
        }));
        ty.get().rw(&mut GIL.lock().unwrap()).base_class = ty.clone();
        ty
    };
    pub static ref SINGLETON_TYPE: Object = Gc::new(GCell::new(TypeClassObject {
        name: "Type".to_string(),
        base_class: OBJECT_TYPE.clone(),
        members: HashMap::new(),
        constructor: &{ |_, _| Ok(SINGLETON_TYPE.clone()) },
    }));
    pub static ref SINGLETON_BUILTIN: Object =
        Gc::new(GCell::new(BuiltinClassObject {}));
}
