use std::{borrow::Borrow, collections::HashMap, convert::TryInto, ops::Deref, vec::Vec};

use enum_dispatch::enum_dispatch;
use lazy_static::lazy_static;
use parking_lot::{Mutex, MutexGuard};
use shredder::{marker::GcDrop, Gc, Scan, ToScan};

use crate::{
    builtins,
    gcell::{GCell, GCellOwner},
};

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

#[enum_dispatch(GcGCellExt)]
#[derive(Clone, Debug, Scan)]
pub enum Object {
    Bool(Gc<GCell<BoolObject>>),
    Float(Gc<GCell<FloatObject>>),
    Function(Gc<GCell<FunctionObject>>),
    Int(Gc<GCell<IntObject>>),
    None(Gc<GCell<NoneObject>>),
    Tuple(Gc<GCell<TupleObject>>),
    Type(Gc<GCell<TypeObject>>),
    Subtype(Gc<GCell<SubtypeObject>>),
}

#[enum_dispatch(ObjectRefTrait)]
#[derive(Debug)]
pub enum ObjectRef<'a> {
    Bool(&'a Gc<GCell<BoolObject>>),
    Float(&'a Gc<GCell<FloatObject>>),
    Function(&'a Gc<GCell<FunctionObject>>),
    Int(&'a Gc<GCell<IntObject>>),
    None(&'a Gc<GCell<NoneObject>>),
    Tuple(&'a Gc<GCell<TupleObject>>),
    Type(&'a Gc<GCell<TypeObject>>),
    Subtype(&'a Gc<GCell<SubtypeObject>>),
}

impl ObjectRef<'_> {
    fn to_owned(self) -> Object {
        match self {
            ObjectRef::Bool(value) => value.clone().into(),
            ObjectRef::Float(value) => value.clone().into(),
            ObjectRef::Function(value) => value.clone().into(),
            ObjectRef::Int(value) => value.clone().into(),
            ObjectRef::None(value) => value.clone().into(),
            ObjectRef::Tuple(value) => value.clone().into(),
            ObjectRef::Type(value) => value.clone().into(),
            ObjectRef::Subtype(value) => value.clone().into(),
        }
    }
}

#[derive(Scan)]
pub struct FunctionObject {
    data: Object,
    func: &'static fn(ObjectRef, ObjectRef, &mut MutexGuard<GCellOwner>, TupleObject) -> ObjectResult,
}

impl ObjectTrait for FunctionObject {
    fn call(
        this: &Gc<GCell<Self>>,
        this_param: ObjectRef,
        gil: &mut MutexGuard<GCellOwner>,
        args: TupleObject,
    ) -> ObjectResult {
        let this = this.get();
        let fun_obj = this.ro(gil);
        let f = *fun_obj.func;
        let data = fun_obj.data.clone();
        f(data.as_ref(), this_param, gil, args)
    }

    fn get_type(&self) -> ObjectRef<'_> {
        todo!()
    }
}

#[enum_dispatch]
pub(crate) trait ObjectRefTrait {}

impl<T: GcGCellExt> ObjectRefTrait for &T {}

impl Object {
    pub fn as_ref(&self) -> ObjectRef<'_> {
        self.into()
    }
}

impl<'a> From<&'a Object> for ObjectRef<'a> {
    fn from(object: &'a Object) -> Self {
        match object {
            &Object::Bool(ref value) => value.into(),
            &Object::Float(ref value) => value.into(),
            &Object::Function(ref value) => value.into(),
            &Object::Int(ref value) => value.into(),
            &Object::None(ref value) => value.into(),
            &Object::Tuple(ref value) => value.into(),
            &Object::Type(ref value) => value.into(),
            &Object::Subtype(ref value) => value.into(),
        }
    }
}

#[derive(Scan)]
pub struct FloatObject {
    pub(crate) value: f64,
}

impl FloatObject {
    pub(crate) fn new(value: f64) -> Self {
        Self { value }
    }
}

impl ObjectTrait for FloatObject {
    fn get_type(&self) -> ObjectRef<'_> {
        G_FLOAT.deref().into()
    }

    // fn into_enum(self) -> ObjectEnum {
    //     ObjectEnum::Float(self)
    // }
}

#[derive(Scan)]
pub struct SubtypeObject {
    base: Object,
    members: HashMap<String, Object>,
}

impl ObjectTrait for SubtypeObject {
    fn get_attr(this: &Gc<GCell<Self>>, name: &str, gil: &GCellOwner) -> ObjectResult {
        let this = this.get();
        let this = this.ro(gil);

        if let Some(value) = this.members.get(name) {
            Ok(value.clone())
        } else {
            this.base.get_attr(gil, name)
        }
    }

    fn get_type(&self) -> ObjectRef {
        self.base.borrow().into()
    }
}

#[enum_dispatch]
pub trait ObjectTrait: GcDrop + Scan + ToScan + Send + Sync + 'static
where
    for<'a> &'a Gc<GCell<Self>>: Into<ObjectRef<'a>>,
{
    fn get_attr(_this: &Gc<GCell<Self>>, name: &str, _gil: &GCellOwner) -> ObjectResult {
        Err(Exception::attribute_error(name))
    }

    fn set_attr(
        _this: &Gc<GCell<Self>>,
        name: &str,
        _value: Object,
        _gil: &GCellOwner,
    ) -> NullResult {
        Err(Exception::attribute_error(name))
    }

    fn del_attr(_this: &Gc<GCell<Self>>, name: &str, _gil: &GCellOwner) -> NullResult {
        Err(Exception::attribute_error(name))
    }

    fn call(
        this: &Gc<GCell<Self>>,
        this_param: ObjectRef,
        gil: &mut MutexGuard<GCellOwner>,
        args: TupleObject,
    ) -> ObjectResult {
        Err(Exception::type_error("Cannot call"))
    }

    fn call_method(
        this: &Gc<GCell<Self>>,
        name: &str,
        gil: &mut MutexGuard<GCellOwner>,
        args: TupleObject,
    ) -> ObjectResult {
        let fun = Self::get_attr(this, name, &gil)?;
        fun.call(this.into(), gil, args)
    }

    fn get_type(&self) -> ObjectRef<'_>;

    fn into_gc(self) -> Object
    where
        Self: Sized + 'static,
        Object: From<Gc<GCell<Self>>>,
    {
        Gc::new(GCell::new(self)).into()
    }
}

#[enum_dispatch]
pub(crate) trait GcGCellExt
where
    for<'a> &'a Self: ObjectRefTrait,
{
    // type InnerType: ObjectTrait + ?Sized;
    // type GetRef<'a>: Deref<Target = GCell<Self::InnerType>> + 'a;
    // fn get(&self) -> Self::GetRef<'_>;
    //
    // type ReadRef<'a>: Deref<Target = Self::InnerType> + 'a;
    // fn ro<'a>(&'a self, gil: &'a GCellOwner) -> Self::ReadRef<'a>;
    //
    // type WriteRef<'a>: DerefMut<Target = Self::InnerType> + 'a;
    // fn rw<'a>(&'a self, gil: &'a mut GCellOwner) -> Self::WriteRef<'a>;
    fn raw_id(&self) -> u64;

    fn get_attr(&self, gil: &GCellOwner, name: &str) -> ObjectResult;

    //noinspection RsSelfConvention
    fn set_attr(&self, gil: &mut GCellOwner, name: &str, value: Object) -> NullResult;

    fn del_attr(&self, gil: &mut GCellOwner, name: &str) -> NullResult;

    fn call(
        &self,
        this_param: ObjectRef,
        gil: &mut MutexGuard<GCellOwner>,
        args: TupleObject,
    ) -> ObjectResult;

    fn call_method(
        &self,
        name: &str,
        gil: &mut MutexGuard<GCellOwner>,
        args: TupleObject,
    ) -> ObjectResult;

    fn get_type(&self, gil: &GCellOwner) -> Object;
}

impl<T> GcGCellExt for Gc<GCell<T>>
where
    T: ObjectTrait + ?Sized,
    for<'a> ObjectRef<'a>: From<&'a Self>,
{
    fn raw_id(&self) -> u64 {
        let ptr = self.get().deref() as *const GCell<_>;
        ptr.to_raw_parts().0 as u64
    }

    fn get_attr(&self, gil: &GCellOwner, name: &str) -> ObjectResult {
        T::get_attr(self, name, gil)
    }

    //noinspection RsSelfConvention
    fn set_attr(&self, gil: &mut GCellOwner, name: &str, value: Object) -> NullResult {
        T::set_attr(self, name, value, gil)
    }

    fn del_attr(&self, gil: &mut GCellOwner, name: &str) -> NullResult {
        T::del_attr(self, name, gil)
    }

    fn call(
        &self,
        this_param: ObjectRef,
        gil: &mut MutexGuard<GCellOwner>,
        args: TupleObject,
    ) -> ObjectResult {
        T::call(self, this_param, gil, args)
    }

    fn call_method(&self, name: &str, gil: &mut MutexGuard<GCellOwner>, args: TupleObject) -> ObjectResult {
        T::call_method(self, name, gil, args)
    }

    fn get_type(&self, gil: &GCellOwner) -> Object {
        self.get().ro(gil).get_type().to_owned()
    }
}

#[derive(Scan)]
pub struct TypeObject {
    pub name: String,
    pub base_class: Option<Object>,
    // pub members: HashMap<String, Object>,
    pub constructor: &'static ObjectSelfFunction,
}
impl ObjectTrait for TypeObject {
    //noinspection RsTypeCheck
    fn get_type(&self) -> ObjectRef<'_> {
        self.base_class
            .as_ref()
            .unwrap_or(OBJECT_TYPE.borrow())
            .into()
    }
}

#[derive(Scan)]
pub struct TupleObject {
    pub data: Vec<Object>,
}

impl ObjectTrait for TupleObject {
    fn get_attr(this: &Gc<GCell<Self>>, name: &str, gil: &GCellOwner) -> ObjectResult {
        if name == "len" {
            match this.get().ro(gil).data.len().try_into() {
                Ok(v) => {
                    let int = IntObject::new(v);
                    Ok(int.into_gc())
                }
                Err(_) => Err(Exception::overflow_error(
                    "could not fit tuple length into integer object",
                )),
            }
        } else {
            Err(Exception::attribute_error(name))
        }
    }

    fn get_type(&self) -> ObjectRef {
        G_TUPLE.deref().into()
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
    fn get_attr(this: &Gc<GCell<Self>>, name: &str, _gil: &GCellOwner) -> ObjectResult {
        if name == "__int__" {
            Ok(FunctionObject {
                data: this.clone().into(),
                func: &{|int: ObjectRef, _: ObjectRef, _: &mut MutexGuard<GCellOwner>, _| {
                    Ok(int.to_owned())
                }},
            }.into_gc())
        } else {
            // unsupported
            todo!()
        }
    }

    fn get_type(&self) -> ObjectRef {
        G_INT.deref().into()
    }
}
impl IntObject {
    pub fn new(data: i64) -> IntObject {
        IntObject { data }
    }
}

#[derive(Scan)]
pub struct NoneObject {}
impl ObjectTrait for NoneObject {
    fn get_type(&self) -> ObjectRef {
        G_NONETYPE.deref().into()
    }

    // fn into_enum(self) -> ObjectEnum {
    //     ObjectEnum::None(self)
    // }
}

#[derive(Scan)]
pub struct BoolObject {
    pub data: bool,
}
impl ObjectTrait for BoolObject {
    fn get_type(&self) -> ObjectRef {
        G_BOOL.deref().into()
    }
}
impl BoolObject {
    pub fn from_bool(data: bool) -> Object {
        if data {
            G_TRUE.clone()
        } else {
            G_FALSE.clone()
        }
    }
}

// If `F` panics, the process will be aborted instead of unwinding
pub fn yield_gil<F>(guard: &mut MutexGuard<GCellOwner>, func: F)
where
    F: FnOnce(),
{
    take_mut::take(guard, |guard| {
        drop(guard);
        func();
        GIL.lock()
    });
}

lazy_static! {
    pub static ref GIL: Mutex<GCellOwner> = Mutex::new(GCellOwner::make());
    pub static ref OBJECT_TYPE: Object = TypeObject {
        name: "Object".to_string(),
        base_class: None,
        // members: HashMap::new(),
        constructor: &{builtins::object_constructor},
    }.into_gc();
    pub static ref G_TYPE: Object = TypeObject {
        name: "type".to_string(),
        base_class: None,
        // members: HashMap::new(),
        constructor: &{builtins::type_constructor},
    }.into_gc();
    pub static ref G_TUPLE: Object = TypeObject {
        name: "tuple".to_string(),
        base_class: None,
        // members: HashMap::new(),
        constructor: &{builtins::tuple_constructor},
    }.into_gc();
    pub static ref G_INT: Object = TypeObject {
        name: "int".to_string(),
        base_class: None,
        // members: HashMap::new(),
        constructor: &{builtins::int_constructor},
    }.into_gc();
    pub static ref G_FLOAT: Object = TypeObject {
        name: "float".to_string(),
        base_class: None,
        // members: HashMap::new(),
        constructor: &{builtins::float_constructor},
    }.into_gc();
    pub static ref G_NONETYPE: Object = TypeObject {
        name: "nonetype".to_string(),
        base_class: None,
        // members: HashMap::new(),
        constructor: &{builtins::nonetype_constructor},
    }
    .into_gc();
    pub static ref G_BOOL: Object = TypeObject {
        name: "bool".to_string(),
        base_class: None,
        // members: HashMap::new(),
        constructor: &{builtins::bool_constructor},
    }.into_gc();
    pub static ref G_NONE: Object = NoneObject {}.into_gc();
    pub static ref G_TRUE: Object = BoolObject { data: true }.into_gc();
    pub static ref G_FALSE: Object = BoolObject { data: false }.into_gc();
}
