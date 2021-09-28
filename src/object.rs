use shredder::{Gc, Scan, ToScan};
use std::collections::HashMap;
use lazy_static::lazy_static;
use std::sync::Mutex;
use shredder::marker::GcDrop;
use std::vec::Vec;

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
            args: vec!(), // TODO make an object out of name
        }
    }

    pub fn type_error(explanation: &str) -> Exception {
        Exception {
            kind: ExceptionKind::TypeError,
            msg: format!("{}", explanation),
            args: vec!(),
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
pub type Object = Gc<dyn ObjectTrait>;

#[derive(Scan)]
pub struct TypeClassObject<F: FnMut(Object, TupleObject) -> Result<Object, Exception>> {
    pub name: String,
    pub base_class: Object,
    pub members: HashMap<String, Object>,
    pub constructor: F,
}
impl ObjectTrait for TypeClassObject<F> {
    fn get_type(&self) -> Object {
        SINGLETON_TYPE.lock().unwrap().clone()
    }
}

#[derive(Scan)]
pub struct TupleObject {
    // Can we make TupleObject unsized somehow and have the data be directly part of the struct, like the C?
    pub data: Vec<Object>,
}
impl ObjectTrait for TypeClassObject {
    fn get_type(&self) -> Object {
        SINGLETON_TYPE.lock().unwrap().clone()
    }
}

#[derive(Scan)]
pub struct BuiltinClassObject {}
impl ObjectTrait for BuiltinClassObject {
    fn get_type(&self) -> Object {
        SINGLETON_TYPE.lock().unwrap().clone()
    }
}

pub struct ClosureObject {}
pub struct ThreadObject {}

pub struct BasicObject {
    pub members: HashMap<String, Object>,
    pub type_: Object,
}

lazy_static! {
    pub static ref SINGLETON_TYPE: Mutex<Object> = Mutex::new(Gc::from_box(Box::new(TypeClassObject{})));
    pub static ref SINGLETON_BUILTIN: Mutex<Object> = Mutex::new(Gc::from_box(Box::new(BuiltinClassObject{})));
}