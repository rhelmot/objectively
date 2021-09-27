use shredder::{Gc, Scan};
use std::collections::HashMap;
use lazy_static::lazy_static;
use std::sync::Mutex;

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

pub trait ObjectTrait: Scan {
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
pub struct TypeClassObject {}
impl ObjectTrait for TypeClassObject {
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
    pub static ref SINGLETON_TYPE: Mutex<Object> = Mutex::new(Gc::new(TypeClassObject{}));
    pub static ref SINGLETON_BUILTIN: Mutex<Object> = Mutex::new(Gc::new(BuiltinClassObject{}));
}