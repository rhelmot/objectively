use std::{collections::HashMap, convert::TryInto, ops::Deref, vec::Vec};
use std::ops::DerefMut;

use enum_dispatch::enum_dispatch;
use lazy_static::lazy_static;
use parking_lot::{Mutex, MutexGuard};
use shredder::{marker::GcDrop, Gc, Scan, ToScan};

use crate::{builtins, gcell::{GCell, GCellOwner}, gcell};

pub type Gil<'a> = MutexGuard<'a, GCellOwner>;
pub type G<T> = Gc<GCell<T>>;
pub type Result<T> = std::result::Result<T, G<ExceptionObject>>;
pub type NullResult = Result<()>;
pub type ObjectResult = Result<Object>;
pub type VecResult = Result<Vec<Object>>;
pub type ObjectSelfFunction = fn(&mut Gil, Object, TupleObject) -> ObjectResult;

#[enum_dispatch(GcGCellExt)]
#[derive(Clone, Debug, Scan)]
pub enum Object {
    Basic(G<BasicObject>),
    Bool(G<BoolObject>),
    Dict(G<DictObject>),
    Float(G<FloatObject>),
    Bytes(G<BytesObject>),
    Function(G<FunctionObject>),
    Int(G<IntObject>),
    None(G<NoneObject>),
    Tuple(G<TupleObject>),
    Type(G<TypeObject>),
    Exception(G<ExceptionObject>),
}

#[enum_dispatch(ObjectRefTrait)]
#[derive(Debug)]
pub enum ObjectRef<'a> {
    Basic(&'a G<BasicObject>),
    Bool(&'a G<BoolObject>),
    Dict(&'a G<DictObject>),
    Float(&'a G<FloatObject>),
    Bytes(&'a G<BytesObject>),
    Function(&'a G<FunctionObject>),
    Int(&'a G<IntObject>),
    None(&'a G<NoneObject>),
    Tuple(&'a G<TupleObject>),
    Type(&'a G<TypeObject>),
    Exception(&'a G<ExceptionObject>),
}

impl ObjectRef<'_> {
    fn to_owned(self) -> Object {
        match self {
            ObjectRef::Basic(value) => value.clone().into(),
            ObjectRef::Bool(value) => value.clone().into(),
            ObjectRef::Dict(value) => value.clone().into(),
            ObjectRef::Float(value) => value.clone().into(),
            ObjectRef::Bytes(value) => value.clone().into(),
            ObjectRef::Function(value) => value.clone().into(),
            ObjectRef::Int(value) => value.clone().into(),
            ObjectRef::None(value) => value.clone().into(),
            ObjectRef::Tuple(value) => value.clone().into(),
            ObjectRef::Type(value) => value.clone().into(),
            ObjectRef::Exception(value) => value.clone().into(),
        }
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
            &Object::Basic(ref value) => value.into(),
            &Object::Dict(ref value) => value.into(),
            &Object::Float(ref value) => value.into(),
            &Object::Bytes(ref value) => value.into(),
            &Object::Function(ref value) => value.into(),
            &Object::Int(ref value) => value.into(),
            &Object::None(ref value) => value.into(),
            &Object::Tuple(ref value) => value.into(),
            &Object::Type(ref value) => value.into(),
            &Object::Exception(ref value) => value.into(),
        }
    }
}

#[derive(Scan)]
pub struct BasicObject {
    pub ty: G<TypeObject>,
    pub dict: HashMap<Vec<u8>, Object>,
}

impl ObjectTrait for BasicObject {
    fn get_type(&self) -> G<TypeObject> {
        self.ty.clone()
    }
}

#[derive(Scan)]
pub struct ExceptionObject {
    pub ty: G<TypeObject>,
    pub args: G<TupleObject>,
}

impl ObjectTrait for ExceptionObject {
    fn get_type(&self) -> G<TypeObject> {
        self.ty.clone()
    }
}

impl ExceptionObject {
    pub fn type_error(msg: &str) -> Result<G<ExceptionObject>> {
        Ok(ExceptionObject {
            ty: G_TYPEERROR.clone(),
            args: TupleObject::new1(Object::Bytes(BytesObject::from_str(msg)?))?,
        }
        .into_gc())
    }

    pub fn attribute_error(attr: &str) -> Result<G<ExceptionObject>> {
        Ok(ExceptionObject {
            ty: G_ATTRIBUTEERROR.clone(),
            args: TupleObject::new1(BytesObject::from_str(attr)?.into())?,
        }
        .into_gc())
    }

    pub fn attribute_error_bytes(attr: &[u8]) -> Result<G<ExceptionObject>> {
        Ok(ExceptionObject {
            ty: G_ATTRIBUTEERROR.clone(),
            args: TupleObject::new1(BytesObject::from_slice(attr)?.into())?,
        }
        .into_gc())
    }

    pub fn overflow_error(msg: &str) -> Result<G<ExceptionObject>> {
        Ok(ExceptionObject {
            ty: G_OVERFLOWERROR.clone(),
            args: TupleObject::new1(BytesObject::from_str(msg)?.into())?,
        }
        .into_gc())
    }

    pub fn runtime_error(msg: &str) -> Result<G<ExceptionObject>> {
        Ok(ExceptionObject {
            ty: G_RUNTIMEERROR.clone(),
            args: TupleObject::new1(BytesObject::from_str(msg)?.into())?,
        }
        .into_gc())
    }

    pub fn value_error(msg: &str) -> Result<G<ExceptionObject>> {
        Ok(ExceptionObject {
            ty: G_VALUEERROR.clone(),
            args: TupleObject::new1(BytesObject::from_str(msg)?.into())?,
        }
        .into_gc())
    }

    pub fn key_error(key: Object) -> Result<G<ExceptionObject>> {
        Ok(ExceptionObject {
            ty: G_KEYERROR.clone(),
            args: TupleObject::new1(key)?
        }.into_gc())
    }
}

#[derive(Scan)]
pub struct DictObject {
    pub ty: G<TypeObject>,
    pub dict: GDict<Object>,
}

pub struct DerefDict<'a>(gcell::GcRef<'a, DictObject>);
pub struct DerefMutDict<'a>(gcell::GcRefMut<'a, DictObject>);

impl <'a> Deref for DerefDict<'a> {
    type Target = GDict<Object>;

    fn deref(&self) -> &Self::Target {
        &self.0.dict
    }
}

impl <'a> Deref for DerefMutDict<'a> {
    type Target = GDict<Object>;

    fn deref(&self) -> &Self::Target {
        &self.0.dict
    }
}
impl <'a> DerefMut for DerefMutDict<'a> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0.dict
    }
}

impl ObjectTrait for DictObject {
    fn get_type(&self) -> G<TypeObject> {
        self.ty.clone()
    }
}

impl DictObject {
    fn to_namespace(&self) -> Result<HashMap<String, Object>> {
        todo!()
    }
}

impl HasDict<Object> for &G<DictObject> {
    type Deref<'a> where Self: 'a = DerefDict<'a>;
    fn get_dict<'a>(&'a self, gil: &'a Gil) -> DerefDict {
        DerefDict(gcell::Ref(self.get(), gil))
    }

    type DerefMut<'a> where Self: 'a = DerefMutDict<'a>;
    fn get_dict_mut<'a>(&'a self, gil: &'a mut Gil) -> DerefMutDict {
        DerefMutDict(gcell::RefMut(self.get(), gil))
    }
}

#[derive(Scan)]
pub struct FunctionObject {
    pub ty: G<TypeObject>,
    data: Object,
    func: &'static fn(
        ObjectRef,
        ObjectRef,
        &mut MutexGuard<GCellOwner>,
        G<TupleObject>,
    ) -> ObjectResult,
}

impl ObjectTrait for FunctionObject {
    fn call(
        this: &G<Self>,
        this_param: ObjectRef,
        gil: &mut MutexGuard<GCellOwner>,
        args: G<TupleObject>,
    ) -> ObjectResult {
        let this = this.get();
        let fun_obj = this.ro(gil);
        let f = *fun_obj.func;
        let data = fun_obj.data.clone();
        f(data.as_ref(), this_param, gil, args)
    }

    fn get_type(&self) -> G<TypeObject> {
        self.ty.clone()
    }
}

#[derive(Scan)]
pub struct FloatObject {
    pub ty: G<TypeObject>,
    pub(crate) value: f64,
}

impl FloatObject {
    pub(crate) fn new(value: f64) -> Result<G<Self>> {
        Ok(Self {
            ty: G_FLOAT.clone(),
            value,
        }
        .into_gc())
    }
}

impl ObjectTrait for FloatObject {
    fn get_type(&self) -> G<TypeObject> {
        self.ty.clone()
    }
}

mod bytes_object {
    use crate::gcell::Immutable;
    use super::*;

    #[derive(Scan)]
    pub struct BytesObject {
        pub ty: G<TypeObject>,
        pub(crate) value: Vec<u8>,
    }

    impl ObjectTrait for BytesObject {
        fn get_type(&self) -> G<TypeObject> {
            self.ty.clone()
        }
    }

    impl BytesObject {
        pub fn from_str(data: &str) -> Result<G<BytesObject>> {
            BytesObject::from_slice(data.as_bytes())
        }

        pub fn from_slice(data: &[u8]) -> Result<G<BytesObject>> {
            Ok(BytesObject {
                ty: G_BYTES.clone(),
                value: data.to_vec(),
            }.into_gc())
        }
    }

    impl PartialEq<&str> for &BytesObject {
        fn eq(&self, other: &&str) -> bool {
            self.value.as_slice() == other.as_bytes()
        }
    }

    unsafe impl Immutable for BytesObject {}
}
pub use bytes_object::BytesObject;
use crate::gdict::{GDict, HasDict};

pub trait ObjectTrait: GcDrop + Scan + ToScan + Send + Sync + 'static {
    fn get_attr_inner(_this: &G<Self>, name: &[u8], _gil: &GCellOwner) -> ObjectResult {
        Err(ExceptionObject::attribute_error_bytes(name)?)
    }

    fn set_attr_inner(
        _this: &G<Self>,
        name: &[u8],
        _value: Object,
        _gil: &GCellOwner,
    ) -> NullResult {
        Err(ExceptionObject::attribute_error_bytes(name)?)
    }

    fn del_attr_inner(_this: &G<Self>, name: &[u8], _gil: &GCellOwner) -> NullResult {
        Err(ExceptionObject::attribute_error_bytes(name)?)
    }

    fn call(
        _this: &G<Self>,
        _this_param: ObjectRef,
        _gil: &mut MutexGuard<GCellOwner>,
        _args: G<TupleObject>,
    ) -> ObjectResult {
        Err(ExceptionObject::type_error("Cannot call")?)
    }

    fn call_method(
        this: &G<Self>,
        name: &str,
        gil: &mut MutexGuard<GCellOwner>,
        args: G<TupleObject>,
    ) -> ObjectResult
    where
        for<'a> &'a G<Self>: Into<ObjectRef<'a>>,
    {
        let fun = Self::get_attr_inner(this, name.as_bytes(), &gil)?;
        fun.call(this.into(), gil, args)
    }

    fn get_type(&self) -> G<TypeObject>;

    fn into_object(self) -> Object
    where
        Self: Sized,
        Object: From<G<Self>>,
    {
        Gc::new(GCell::new(self)).into()
    }

    fn into_gc(self) -> G<Self>
    where
        Self: Sized,
    {
        Gc::new(GCell::new(self))
    }
}

#[enum_dispatch]
pub(crate) trait GcGCellExt
where
    for<'a> &'a Self: ObjectRefTrait,
{
    fn raw_id(&self) -> u64;

    fn is(&self, other: &impl GcGCellExt) -> bool {
        self.raw_id() == other.raw_id()
    }

    fn get_attr_inner(&self, gil: &GCellOwner, name: &[u8]) -> ObjectResult;
    fn set_attr_inner(&self, gil: &mut GCellOwner, name: &[u8], value: Object) -> NullResult;
    fn del_attr_inner(&self, gil: &mut GCellOwner, name: &[u8]) -> NullResult;

    fn get_attr_str(&self, gil: &GCellOwner, name: &str) -> ObjectResult {
        Ok(self.get_attr_inner(gil, name.as_bytes())?)
    }
    fn set_attr_str(&self, gil: &mut GCellOwner, name: &str, value: Object) -> NullResult {
        Ok(self.set_attr_inner(gil, name.as_bytes(), value)?)
    }
    fn del_attr_str(&self, gil: &mut GCellOwner, name: &str) -> NullResult {
        Ok(self.del_attr_inner(gil, name.as_bytes())?)
    }

    fn get_attr(&self, gil: &GCellOwner, name: ObjectRef) -> ObjectResult {
        if let ObjectRef::Bytes(b) = name {
            self.get_attr_inner(gil, b.get().unwrap_ref().value.as_slice())
        } else {
            Err(ExceptionObject::type_error("Expected bytes")?)
        }
    }

    fn set_attr(&self, gil: &mut GCellOwner, name: ObjectRef, value: Object) -> NullResult {
        if let ObjectRef::Bytes(b) = name {
            self.set_attr_inner(gil, b.get().unwrap_ref().value.as_slice(), value)
        } else {
            Err(ExceptionObject::type_error("Expected bytes")?)
        }
    }

    fn del_attr(&self, gil: &mut GCellOwner, name: ObjectRef) -> NullResult {
        if let ObjectRef::Bytes(b) = name {
            self.del_attr_inner(gil, b.get().unwrap_ref().value.as_slice())
        } else {
            Err(ExceptionObject::type_error("Expected bytes")?)
        }
    }

    fn call(
        &self,
        this_param: ObjectRef,
        gil: &mut MutexGuard<GCellOwner>,
        args: G<TupleObject>,
    ) -> ObjectResult;

    fn call_method(
        &self,
        name: &str,
        gil: &mut MutexGuard<GCellOwner>,
        args: G<TupleObject>,
    ) -> ObjectResult;

    fn get_type(&self, gil: &GCellOwner) -> G<TypeObject>;
}

impl<T> GcGCellExt for G<T>
where
    T: ObjectTrait,
    for<'a> ObjectRef<'a>: From<&'a Self>,
{
    fn raw_id(&self) -> u64 {
        let ptr = (&*self.get()) as *const GCell<T>;
        ptr.to_raw_parts().0 as u64
    }

    fn get_attr_inner(&self, gil: &GCellOwner, name: &[u8]) -> ObjectResult {
        T::get_attr_inner(self, name, gil)
    }

    fn set_attr_inner(&self, gil: &mut GCellOwner, name: &[u8], value: Object) -> NullResult {
        T::set_attr_inner(self, name, value, gil)
    }

    fn del_attr_inner(&self, gil: &mut GCellOwner, name: &[u8]) -> NullResult {
        T::del_attr_inner(self, name, gil)
    }

    fn call(
        &self,
        this_param: ObjectRef<'_>,
        gil: &mut MutexGuard<GCellOwner>,
        args: G<TupleObject>,
    ) -> ObjectResult {
        T::call(self, this_param, gil, args)
    }

    fn call_method(
        &self,
        name: &str,
        gil: &mut MutexGuard<GCellOwner>,
        args: G<TupleObject>,
    ) -> ObjectResult {
        T::call_method(self, name, gil, args)
    }

    fn get_type(&self, gil: &GCellOwner) -> G<TypeObject> {
        self.get().ro(gil).get_type().to_owned()
    }
}

#[derive(Scan)]
pub struct TypeObject {
    pub ty: Option<G<TypeObject>>,
    pub base_class: Option<G<TypeObject>>,
    // pub members: HashMap<String, Object>,
    pub constructor: &'static ObjectSelfFunction,
}
impl ObjectTrait for TypeObject {
    fn get_type(&self) -> G<TypeObject> {
        self.ty.clone().unwrap_or_else(|| G_TYPE.clone())
    }
}

#[derive(Scan)]
pub struct TupleObject {
    pub ty: G<TypeObject>,
    pub data: Vec<Object>,
}

impl ObjectTrait for TupleObject {
    fn get_attr_inner(this: &G<Self>, name: &[u8], gil: &GCellOwner) -> ObjectResult {
        if name == "len".as_bytes() {
            match this.get().ro(gil).data.len().try_into() {
                Ok(v) => {
                    let int = IntObject::new(v)?;
                    Ok(int.into())
                }
                Err(_) => Err(ExceptionObject::overflow_error(
                    "could not fit tuple length into integer object",
                )?),
            }
        } else {
            Err(ExceptionObject::attribute_error_bytes(name)?)
        }
    }

    fn get_type(&self) -> G<TypeObject> {
        self.ty.clone()
    }
}
impl TupleObject {
    pub fn new(args: Vec<Object>) -> Result<G<TupleObject>> {
        Ok(TupleObject {
            ty: G_TUPLE.clone(),
            data: args,
        }
       .into_gc())
    }

    pub fn new0() -> Result<G<TupleObject>> {
        Ok(TupleObject {
            ty: G_TUPLE.clone(),
            data: vec![],
        }
        .into_gc())
    }

    pub fn new1(arg0: Object) -> Result<G<TupleObject>> {
        Ok(TupleObject {
            ty: G_TUPLE.clone(),
            data: vec![arg0],
        }
        .into_gc())
    }

    pub fn new2(arg0: Object, arg1: Object) -> Result<G<TupleObject>> {
        Ok(TupleObject {
            ty: G_TUPLE.clone(),
            data: vec![arg0, arg1],
        }
        .into_gc())
    }

    pub fn new3(arg0: Object, arg1: Object, arg2: Object) -> Result<G<TupleObject>> {
        Ok(TupleObject {
            ty: G_TUPLE.clone(),
            data: vec![arg0, arg1, arg2],
        }
        .into_gc())
    }

    pub fn new4(arg0: Object, arg1: Object, arg2: Object, arg3: Object) -> Result<G<TupleObject>> {
        Ok(TupleObject {
            ty: G_TUPLE.clone(),
            data: vec![arg0, arg1, arg2, arg3],
        }
        .into_gc())
    }
}

#[derive(Scan)]
pub struct IntObject {
    pub ty: G<TypeObject>,
    pub data: i64,
}
impl ObjectTrait for IntObject {
    fn get_attr_inner(this: &G<Self>, name: &[u8], gil: &GCellOwner) -> ObjectResult {
        if name == b"__int__" {
            Ok(FunctionObject {
                ty: G_FUNCTION.clone(),
                data: this.clone().into(),
                func: &{
                    |int: ObjectRef, _: ObjectRef, _: &mut MutexGuard<GCellOwner>, _| {
                        Ok(int.to_owned())
                    }
                },
            }
            .into_object())
        } else {
            Err(ExceptionObject::attribute_error_bytes(name)?)
        }
    }

    fn get_type(&self) -> G<TypeObject> {
        self.ty.clone()
    }
}
impl IntObject {
    pub fn new(data: i64) -> Result<G<IntObject>> {
        Ok(IntObject {
            ty: G_INT.clone(),
            data,
        }.into_gc())
    }
}

#[derive(Scan)]
pub struct NoneObject {
}

impl ObjectTrait for NoneObject {
    fn get_type(&self) -> G<TypeObject> {
        G_NONETYPE.clone()
    }
}

#[derive(Scan)]
pub struct BoolObject {}
impl ObjectTrait for BoolObject {
    fn get_type(&self) -> G<TypeObject> {
        G_BOOL.clone()
    }
}
impl BoolObject {
    pub fn new(data: bool) -> Result<G<BoolObject>> {
        Ok(if data {
            G_TRUE.clone()
        } else {
            G_FALSE.clone()
        })
    }
}
pub fn bool_raw(b: G<BoolObject>) -> bool {
    b.is(G_TRUE.deref())
}

// If `F` panics, the process will be aborted instead of unwinding
pub fn yield_gil<F>(guard: &mut MutexGuard<GCellOwner>, func: F)
where
    F: FnOnce(),
{
    MutexGuard::unlocked(guard, func);
}


lazy_static! {
    pub static ref GIL: Mutex<GCellOwner> = Mutex::new(GCellOwner::make());
    pub static ref OBJECT_TYPE: G<TypeObject> = TypeObject {
        ty: None,
        base_class: None,
        // members: HashMap::new(),
        constructor: &{builtins::object_constructor},
    }.into_gc();
    pub static ref G_TYPE: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(OBJECT_TYPE.clone()),
        // members: HashMap::new(),
        constructor: &{builtins::type_constructor},
    }.into_gc();
    pub static ref G_TUPLE: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(OBJECT_TYPE.clone()),
        // members: HashMap::new(),
        constructor: &{builtins::tuple_constructor},
    }.into_gc();
    pub static ref G_INT: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(OBJECT_TYPE.clone()),
        // members: HashMap::new(),
        constructor: &{builtins::int_constructor},
    }.into_gc();
    pub static ref G_FLOAT: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(OBJECT_TYPE.clone()),
        // members: HashMap::new(),
        constructor: &{builtins::float_constructor},
    }.into_gc();
    pub static ref G_BYTES: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(OBJECT_TYPE.clone()),
        // members: HashMap::new(),
        constructor: &{builtins::bytes_constructor},
    }.into_gc();
    pub static ref G_NONETYPE: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(OBJECT_TYPE.clone()),
        // members: HashMap::new(),
        constructor: &{builtins::nonetype_constructor},
    }
    .into_gc();
    pub static ref G_BOOL: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(OBJECT_TYPE.clone()),
        // members: HashMap::new(),
        constructor: &{builtins::bool_constructor},
    }.into_gc();
    pub static ref G_EXCEPTION: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(OBJECT_TYPE.clone()),
        constructor: &{builtins::exc_constructor},
    }.into_gc();
    pub static ref G_TYPEERROR: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(G_EXCEPTION.clone()),
        constructor: &{builtins::exc_constructor},
    }.into_gc();
    pub static ref G_ATTRIBUTEERROR: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(G_EXCEPTION.clone()),
        constructor: &{builtins::exc_constructor},
    }.into_gc();
    pub static ref G_OVERFLOWERROR: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(G_EXCEPTION.clone()),
        constructor: &{builtins::exc_constructor},
    }.into_gc();
    pub static ref G_RUNTIMEERROR: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(G_EXCEPTION.clone()),
        constructor: &{builtins::exc_constructor},
    }.into_gc();
    pub static ref G_MEMORYERROR: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(G_EXCEPTION.clone()),
        constructor: &{builtins::exc_constructor},
    }.into_gc();
    pub static ref G_VALUEERROR: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(G_EXCEPTION.clone()),
        constructor: &{builtins::exc_constructor},
    }.into_gc();
    pub static ref G_KEYERROR: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(G_EXCEPTION.clone()),
        constructor: &{builtins::exc_constructor},
    }.into_gc();
    pub static ref G_STOPITERATION: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(G_EXCEPTION.clone()),
        constructor: &{builtins::exc_constructor},
    }.into_gc();
    pub static ref G_FUNCTION: G<TypeObject> = TypeObject {
        ty: None,
        base_class: Some(OBJECT_TYPE.clone()),
        constructor: &{builtins::null_constructor},
    }.into_gc();
    pub static ref MEMORYERROR_INST: G<ExceptionObject> = ExceptionObject {
        ty: G_MEMORYERROR.clone(),
        args: TupleObject::new1(BytesObject::from_str("Out of memory").unwrap().into()).unwrap(),
    }.into_gc();
    pub static ref G_NONE: G<NoneObject> = NoneObject {}.into_gc();
    pub static ref G_TRUE: G<BoolObject> = BoolObject {}.into_gc();
    pub static ref G_FALSE: G<BoolObject> = BoolObject {}.into_gc();
}
