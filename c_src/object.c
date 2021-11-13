#include <string.h>
#include <stdio.h>
#include <alloca.h>

#include "object.h"
#include "gc.h"
#include "errors.h"
#include "interpreter.h"
#include "thread.h"

// tables and instances
ObjectTable null_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = null_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.given = sizeof(EmptyObject),
};
ObjectTable dicto_table = {
	.trace = dicto_trace,
	.finalize = dicto_finalize,
	.get_attr = dicto_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.computed = dicto_size,
};
ObjectTable object_table = {
	.trace = object_trace,
	.finalize = object_finalize,
	.get_attr = object_get_attr,
	.set_attr = object_set_attr,
	.del_attr = object_del_attr,
	.call = object_call,
	.size.computed = object_size,
};
ObjectTable type_table = {
	.trace = type_trace,
	.finalize = null_finalize,
	.get_attr = type_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = type_call,
	.size.computed = type_size,
};
ObjectTable int_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = null_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.given = sizeof(IntObject),
};
ObjectTable float_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = null_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.given = sizeof(FloatObject),
};
ObjectTable list_table = {
	.trace = list_trace,
	.finalize = list_finalize,
	.get_attr = list_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.computed = list_size,
};
ObjectTable tuple_table = {
	.trace = tuple_trace,
	.finalize = null_finalize,
	.get_attr = tuple_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.computed = tuple_size,
};
ObjectTable bytes_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = bytes_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.computed = bytes_size,
};
ObjectTable bytes_unowned_table = {
	.trace = bytes_unowned_trace,
	.finalize = null_finalize,
	.get_attr = bytes_unowned_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.given = sizeof(BytesUnownedObject),
};
ObjectTable bytearray_table = {
	.trace = null_trace,
	.finalize = bytearray_finalize,
	.get_attr = bytes_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.computed = bytearray_size,
};
ObjectTable builtinfunction_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = null_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = builtinfunction_call,
	.size.given = sizeof(BuiltinFunctionObject),
};
ObjectTable closure_table = {
	.trace = closure_trace,
	.finalize = null_finalize,
	.get_attr = closure_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = closure_call,
	.size.given = sizeof(ClosureObject),
};
ObjectTable boundmeth_table = {
	.trace = boundmeth_trace,
	.finalize = null_finalize,
	.get_attr = boundmeth_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = boundmeth_call,
	.size.given = sizeof(BoundMethodObject),
};
ObjectTable slice_table = {
	.trace = slice_trace,
	.finalize = null_finalize,
	.get_attr = slice_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.given = sizeof(SliceObject),
};
ObjectTable exc_table = {
	.trace = exc_trace,
	.finalize = null_finalize,
	.get_attr = exc_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.given = sizeof(ExceptionObject),
};

// builtin type class instances


Object *object_constructor(Object *self, TupleObject *args);
TypeObject g_object = {
	.header_basic.header_dict.header.table = &type_table,
	.header_basic.header_dict.header.type = &g_type,
	.base_class = NULL,
	.constructor = object_constructor,
};
STATIC_OBJECT(g_object);
ADD_MEMBER(builtins, "object", g_object);

BUILTIN_TYPE(type, object, type_constructor);
BUILTIN_TYPE(nonetype, object, nonetype_constructor);
BUILTIN_TYPE(int, object, int_constructor);
BUILTIN_TYPE(float, object, float_constructor);
BUILTIN_TYPE(bool, object, bool_constructor);
BUILTIN_TYPE(bytes, object, bytes_constructor);
BUILTIN_TYPE(bytearray, object, bytearray_constructor);
BUILTIN_TYPE(dict, object, dict_constructor);
BUILTIN_TYPE(list, object, list_constructor);
BUILTIN_TYPE(tuple, object, tuple_constructor);
BUILTIN_TYPE(builtin, object, null_constructor);
BUILTIN_TYPE(closure, object, closure_constructor);
BUILTIN_TYPE(boundmeth, object, null_constructor);
BUILTIN_TYPE(slice, object, slice_constructor);
BUILTIN_TYPE(exception, object, exc_constructor);

EmptyObject g_none = {
	.header.type = &g_nonetype,
	.header.table = &null_table,
};
STATIC_OBJECT(g_none);
ADD_MEMBER(builtins, "none", g_none);
EmptyObject g_true = {
	.header.type = &g_bool,
	.header.table = &null_table,
};
STATIC_OBJECT(g_true);
ADD_MEMBER(builtins, "true", g_true);
EmptyObject g_false = {
	.header.type = &g_bool,
	.header.table = &null_table,
};
STATIC_OBJECT(g_false);
ADD_MEMBER(builtins, "false", g_false);
TupleObject empty_tuple = {
	.header.type = &g_tuple,
	.header.table = &tuple_table,
	.len = 0,
};
STATIC_OBJECT(empty_tuple);

bool null_trace(Object *self, bool (*tracer)(Object *tracee)) {
	return true;
}
void null_finalize(Object *self) {
	return;
}
Object *null_get_attr(Object *self, Object *name) {
	error = exc_arg(&g_AttributeError, name);
	return NULL;
}
bool null_set_attr(Object *self, Object *name, Object *value) {
	error = exc_arg(&g_AttributeError, name);
	return NULL;
}
bool null_del_attr(Object *self, Object *name) {
	error = exc_arg(&g_AttributeError, name);
	return NULL;
}
Object *null_call(Object *self, TupleObject *args) {
	error = exc_msg(&g_TypeError, "Cannot call");
	return NULL;
}

IntObject *int_raw(int64_t value) {
	IntObject *result = (IntObject *)gc_alloc(sizeof(IntObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.table = &int_table;
	result->header.type = &g_int;
	result->value = value;
	return result;
}

IntObject *int_raw_ex(int64_t value, TypeObject *type) {
	IntObject *result = int_raw(value);
	if (result == NULL) {
		return NULL;
	}
	result->header.type = type;
	return result;
}

EmptyObject *bool_raw(bool value) {
	return value ? &g_true : &g_false;
}

FloatObject *float_raw(double value) {
	FloatObject *result = (FloatObject*)gc_alloc(sizeof(IntObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.table = &float_table;
	result->header.type = &g_float;
	result->value = value;
	return result;
}

FloatObject *float_raw_ex(double value, TypeObject *type) {
	FloatObject *result = float_raw(value);
	if (result == NULL) {
		return NULL;
	}
	result->header.type = type;
	return result;
}

ListObject *list_raw(Object **data, size_t len) {
	ListObject *result = (ListObject*)gc_alloc(sizeof(ListObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.type = &g_list;
	result->header.table = &list_table;
	result->len = len;
	result->cap = len;
	result->data = current_thread_alloc(sizeof(Object*) * len);
	if (!result->data) {
		error = (Object*)&MemoryError_inst;
		current_thread_dealloc(result, sizeof(ListObject));
		return NULL;
	}
	memcpy(result->data, data, sizeof(Object*) * len);
	return result;
}

ListObject *list_raw_ex(Object **data, size_t len, TypeObject *type) {
	ListObject *result = list_raw(data, len);
	if (result == NULL) {
		return NULL;
	}
	result->header.type = type;
	return result;
}

TupleObject *tuple_raw(Object **data, size_t len) {
	if (len == 0) {
		return &empty_tuple;
	}
	TupleObject *result = (TupleObject*)gc_alloc(sizeof(TupleObject) + sizeof(Object*) * len);
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.type = &g_tuple;
	result->header.table = &tuple_table;
	result->len = len;
	if (data != NULL) {
		memcpy(result->data, data, sizeof(Object*) * len);
	}
	return result;
}

TupleObject *tuple_raw_ex(Object **data, size_t len, TypeObject *type) {
	// duplicated logic to avoid the empty_tuple singleton
	TupleObject *result = (TupleObject*)gc_alloc(sizeof(TupleObject) + sizeof(Object*) * len);
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.type = type;
	result->header.table = &tuple_table;
	result->len = len;
	if (data != NULL) {
		memcpy(result->data, data, sizeof(Object*) * len);
	}
	return result;
}

BytesObject *bytes_raw(const char *data, size_t len) {
	BytesObject *result = (BytesObject*)gc_alloc(sizeof(BytesObject) + len * sizeof(char));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.type = &g_bytes;
	result->header.table = &bytes_table;
	result->len = len;
	if (data != NULL) {
		memcpy(result->_data, data, len * sizeof(char));
	}
	return result;
}

BytesObject *bytes_raw_ex(const char *data, size_t len, TypeObject *type) {
	BytesObject *result = bytes_raw(data, len);
	if (result == NULL) {
		return NULL;
	}
	result->header.type = type;
	return result;
}

BytesUnownedObject *bytes_unowned_raw(const char *data, size_t len, Object *owner) {
	BytesUnownedObject *result = (BytesUnownedObject*)gc_alloc(sizeof(BytesUnownedObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}

	result->header_bytes.header.type = &g_bytes;
	result->header_bytes.header.table = &bytes_unowned_table;
	result->header_bytes.len = len;
	result->_data = data;
	result->owner = owner;
	return result;
}

BytesUnownedObject *bytes_unowned_raw_ex(const char *data, size_t len, Object *owner, TypeObject *type) {
	BytesUnownedObject *result = bytes_unowned_raw(data, len, owner);
	if (result == NULL) {
		return NULL;
	}
	result->header_bytes.header.type = type;
	return result;
}

BytearrayObject *bytearray_raw(const char *data, size_t len, TypeObject *type) {
	BytearrayObject *result = (BytearrayObject*)gc_alloc(sizeof(BytearrayObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}

	result->header_bytes.header.type = type;
	result->header_bytes.header.table = &bytearray_table;
	result->header_bytes.len = len;
	result->data = current_thread_alloc(len);
	if (result->data == NULL) {
		current_thread_dealloc(result, sizeof(BytearrayObject));
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	if (data != NULL) {
		memcpy(result->data, data, len);
	}
	result->cap = len;
	return result;
}

DictObject *dicto_raw() {
	DictObject *result = (DictObject *)gc_alloc(sizeof(DictObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.type = &g_dict;
	result->header.table = &dicto_table;
	return result;
}

DictObject *dicto_raw_ex(TypeObject *type) {
	DictObject *result = dicto_raw();
	if (result == NULL) {
		return NULL;
	}
	result->header.type = type;
	return result;
}

BasicObject *object_raw(TypeObject *type) {
	BasicObject *result = (BasicObject*)gc_alloc(sizeof(BasicObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header_dict.header.type = type;
	result->header_dict.header.table = &object_table;
	return result;
}

ClosureObject *closure_raw(BytesObject *bytecode, DictObject *context) {
	ClosureObject *result = (ClosureObject*)gc_alloc(sizeof(ClosureObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.type = &g_closure;
	result->header.table = &closure_table;
	result->bytecode = bytecode;
	result->context = context;
	return result;
}

ClosureObject *closure_raw_ex(BytesObject *bytecode, DictObject *context, TypeObject *type) {
	ClosureObject *result = closure_raw(bytecode, context);
	if (result == NULL) {
		return NULL;
	}
	result->header.type = type;
	return result;
}

BoundMethodObject *boundmeth_raw(Object *meth, Object *self) {
	BoundMethodObject *result = (BoundMethodObject*)gc_alloc(sizeof(BoundMethodObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.type = &g_boundmeth;
	result->header.table = &boundmeth_table;
	result->method = meth;
	result->self = self;
	return result;
}

SliceObject *slice_raw(Object *start, Object *end) {
	SliceObject *result = (SliceObject*)gc_alloc(sizeof(SliceObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.type = &g_slice;
	result->header.table = &slice_table;
	result->start = start;
	result->end = end;
	return result;
}

SliceObject *slice_raw_ex(Object *start, Object *end, TypeObject *type) {
	SliceObject *result = slice_raw(start, end);
	if (result == NULL) {
		return NULL;
	}
	result->header.type = type;
	return result;
}


ExceptionObject *exc_raw(TypeObject *type, TupleObject *args) {
	ExceptionObject *result = (ExceptionObject*)gc_alloc(sizeof(ExceptionObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header.type = type;
	result->header.table = &exc_table;
	result->args = args;
	return result;
}

// MAKE SURE YOU FREE THE RESULT OF THIS! IT IS NOT GARBAGE COLLECTED!
char *c_str(Object *arg) {
	if (!isinstance_inner(arg, &g_bytes) && !isinstance_inner(arg, &g_bytearray)) {
		error = exc_msg(&g_TypeError, "Cannot use object as string");
		return NULL;
	}
	BytesObject *str = (BytesObject*)arg;
	if (memchr(bytes_data(str), '\0', str->len) != NULL) {
		error = exc_msg(&g_ValueError, "Embedded null byte");
		return NULL;
	}
	return strndup(bytes_data(str), str->len);
}

bool object_equals_str(Object *obj, char *str) {
	char *maybe_str = c_str(obj);
	if (maybe_str == NULL) {
		error = NULL;
		return false;
	}
	bool result = strcmp(maybe_str, str) == 0;
	free(maybe_str);
	return result;
}

IntObject *bytes2int(BytesObject *arg, int64_t base, TypeObject *type) {
	char *buffer = c_str((Object*)arg);
	if (buffer == NULL) {
		return NULL;
	}
	char *endptr;
	int64_t value = strtoll(buffer, &endptr, base);
	bool valid = buffer[0] != 0 && endptr[0] == 0;
	free(buffer);
	if (!valid) {
		error = exc_msg(&g_ValueError, "invalid string for integer conversion");
		return NULL;
	}
	return int_raw_ex(value, type);
}

FloatObject *bytes2float(BytesObject *arg, TypeObject *type) {
	char *buffer = c_str((Object*)arg);
	if (buffer == NULL) {
		return NULL;
	}
	char *endptr;
	double value = strtod(buffer, &endptr);
	bool valid = buffer[0] != 0 && endptr[0] == 0;
	free(buffer);
	if (!valid) {
		error = exc_msg(&g_ValueError, "invalid string for float conversion");
		return NULL;
	}
	return float_raw_ex(value, type);
}

Object* int_constructor(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject*)_self;
	if (args->len == 0) {
		return (Object*)int_raw(0);
	} else if (args->len == 1) {
		Object *arg = args->data[0];
		if (isinstance_inner(arg, &g_int)) {
			return (Object*)int_raw_ex(((IntObject*)arg)->value, self);
		} else if (isinstance_inner(arg, &g_float)) {
			return (Object*)int_raw_ex((int64_t)((FloatObject*)arg)->value, self);
		} else if (isinstance_inner(arg, &g_bytes) || isinstance_inner(arg, &g_bytearray)) {
			return (Object*)bytes2int((BytesObject*)arg, 10, self);
		} else {
			error = exc_msg(&g_TypeError, "Expected int, float, or bytes");
			return NULL;
		}
	} else if (args->len == 2) {
		Object *arg0 = args->data[0];
		Object *arg1 = args->data[1];
		if ((isinstance_inner(arg0, &g_bytes) || isinstance_inner(arg0, &g_bytearray)) && isinstance_inner(arg1, &g_int)) {
			return (Object*)bytes2int((BytesObject*)arg0, ((IntObject*)arg1)->value, self);
		} else {
			error = exc_msg(&g_TypeError, "Expected bytes and int");
			return NULL;
		}
	} else {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 or 2 arguments");
		return NULL;
	}
}

Object *float_constructor(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject*)_self;
	if (args->len == 0) {
		return (Object*)float_raw(0.);
	} else if (args->len == 1) {
		Object *arg = args->data[0];
		if (isinstance_inner(arg, &g_float)) {
			return (Object*)float_raw_ex(((FloatObject*)arg)->value, self);
		} else if (isinstance_inner(arg, &g_int)) {
			return (Object*)float_raw_ex((double)((IntObject*)arg)->value, self);
		} else if (isinstance_inner(arg, &g_bytes) || isinstance_inner(arg, &g_bytearray)) {
			return (Object*)bytes2float((BytesObject*)arg, self);
		} else {
			error = exc_msg(&g_TypeError, "Expected float or int or bytes");
			return NULL;
		}
	} else {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 arguments");
		return NULL;
	}
}

Object *object_constructor(Object *_self, TupleObject *args) {
	if (_self->table != &type_table) {
		puts("Fatal: object constructor called with something that is not a type");
		abort();
	}
	TypeObject *self = (TypeObject*)_self;

	BasicObject *result = object_raw(self);
	if (result == NULL) {
		return NULL;
	}
	gc_root((Object*)result);
	Object *init = get_attr_inner((Object*)result, "__init__");
	if (init) {
		if (!call(init, args)) {
			gc_unroot((Object*)result);
			return NULL;
		}
	}
	gc_unroot((Object*)result);
	return (Object*)result;
}

size_t object_size(Object *_self) {
	BasicObject *self = (BasicObject*)_self;
	return sizeof(BasicObject) + dict_size(&self->header_dict.core);
}

bool object_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	BasicObject *self = (BasicObject*)_self;
	bool inner_tracer(void *key, void **val) {
		if (!tracer(key)) return false;
		if (!tracer(*val)) return false;
		return true;
	}
	return dict_trace(&self->header_dict.core, inner_tracer);
}

void object_finalize(Object *_self) {
	BasicObject *self = (BasicObject*)_self;
	void other_dealloc(void * ptr, size_t size) { quota_dealloc(ptr, size, self->header_dict.header.group); }
	dict_destruct(&self->header_dict.core, other_dealloc);
}

HashResult string_hasher(void *val) {
	// fnv-1a-64
	// http://isthe.com/chongo/tech/comp/fnv/
	uint64_t hash = 14695981039346656037ull;
	for (char *s = val; *s; s++) {
		hash ^= *s;
		hash *= 1099511628211ull;
	}
	return (HashResult) {
		.hash = hash,
		.success = true,
	};
}

EqualityResult string_equals(void *val1, void *val2) {
	return (EqualityResult) {
		.equals = strcmp(val1, val2) == 0,
		.success = true,
	};
}

Object *object_get_attr(Object *_self, Object *name) {
	BasicObject *self = (BasicObject*)_self;
	GetResult result = dict_get(&self->header_dict.core, (void*)name, object_hasher, object_equals);
	if (!result.success) {
		return NULL;
	}
	if (!result.found) {
		error = exc_arg(&g_AttributeError, name);
		return NULL;
	}
	return (Object*)result.val;
}

bool object_set_attr(Object *_self, Object *name, Object *val) {
	BasicObject *self = (BasicObject*)_self;
	if (CURRENT_GROUP != self->header_dict.header.group && !dict_get(&self->header_dict.core, (void*)name, object_hasher, object_equals).found) {
		error = exc_msg(&g_RuntimeError, "Cannot allocate space in another group");
		return NULL;
	}
	void *other_alloc(size_t size) { return quota_alloc(size, self->header_dict.header.group); }
	void other_dealloc(void * ptr, size_t size) { quota_dealloc(ptr, size, self->header_dict.header.group); }
	return dict_set(&self->header_dict.core, (void*)name, (void*)val, object_hasher, object_equals, other_alloc, other_dealloc);
}

bool object_del_attr(Object *_self, Object *name) {
	BasicObject *self = (BasicObject*)_self;
	void other_dealloc(void * ptr, size_t size) { quota_dealloc(ptr, size, self->header_dict.header.group); }
	GetResult result = dict_pop(&self->header_dict.core, (void*)name, object_hasher, object_equals, other_dealloc);
	if (!result.success) {
		return false;
	}
	if (!result.found) {
		error = exc_arg(&g_AttributeError, name);
		return false;
	}
	return true;
}

Object *object_call(Object *_self, TupleObject *args) {
	Object *method = get_attr_inner(_self, "__call__");
	if (!method) {
		return NULL;
	}
	return call(method, args);
}

size_t dicto_size(Object *_self) {
	DictObject *self = (DictObject*)_self;
	return sizeof(DictObject) + dict_size(&self->core);
}

bool dicto_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	DictObject *self = (DictObject*)_self;
	bool inner_tracer(void *key, void **val) {
		if (!tracer((Object*)key)) return false;
		if (!tracer((Object*)*val)) return false;
		return true;
	}
	return dict_trace(&self->core, inner_tracer);
}

void dicto_finalize(Object *_self) {
	DictObject *self = (DictObject*)_self;
	void other_dealloc(void * ptr, size_t size) { quota_dealloc(ptr, size, self->header.group); }
	dict_destruct(&self->core, other_dealloc);
}

Object *dicto_get_attr(Object *_self, Object *name) {
	DictObject *self = (DictObject*)_self;
	if (object_equals_str(name, "len")) {
		return (Object*)int_raw(self->core.len);
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *tuple_constructor(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject *)_self;
	if (args->len == 0) {
		return (Object*)tuple_raw_ex(NULL, 0, self);
	} else if (args->len == 1) {
		Object *arg = args->data[0];
		if (isinstance_inner(arg, &g_tuple)) {
			return arg;
		} else if (isinstance_inner(arg, &g_list)) {
			return (Object*)tuple_raw_ex(((ListObject*)arg)->data, ((ListObject*)arg)->len, self);
		} else {
			error = exc_msg(&g_TypeError, "Expected tuple or list");
			return NULL;
		}
	} else {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 args");
		return NULL;
	}
}

size_t tuple_size(Object *_self) {
	TupleObject *self = (TupleObject*)_self;
	return sizeof(TupleObject) + self->len * sizeof(Object*);
}

bool tuple_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	TupleObject *self = (TupleObject*)_self;
	for (size_t i = 0; i < self->len; i++) {
		if (!tracer(self->data[i])) {
			return false;
		}
	}
	return true;
}

Object *tuple_get_attr(Object *_self, Object *name) {
	TupleObject *self = (TupleObject*)_self;
	if (object_equals_str(name, "len")) {
		return (Object*)int_raw(self->len);
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *list_constructor(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject*)_self;
	if (args->len == 0) {
		return (Object*)list_raw_ex(NULL, 0, self);
	} else if (args->len == 1) {
		Object *arg = args->data[0];
		if (isinstance_inner(arg, &g_tuple)) {
			return (Object*)list_raw_ex(((TupleObject*)arg)->data, ((TupleObject*)arg)->len, self);
		} else if (isinstance_inner(arg, &g_list)) {
			return (Object*)list_raw_ex(((ListObject*)arg)->data, ((ListObject*)arg)->len, self);
		} else {
			error = exc_msg(&g_TypeError, "Expected tuple or list");
			return NULL;
		}
	} else {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 args");
		return NULL;
	}
}

size_t list_size(Object *_self) {
	ListObject *self = (ListObject*)_self;
	return sizeof(ListObject) + sizeof(Object*) * self->cap;
}

bool list_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	ListObject *self = (ListObject*)_self;
	for (size_t i = 0; i < self->len; i++) {
		if (!tracer(self->data[i])) {
			return false;
		}
	}
	return true;
}

void list_finalize(Object *_self) {
	ListObject *self = (ListObject*)_self;
	quota_dealloc(self->data, sizeof(Object*) * self->cap, self->header.group);
	self->data = NULL;
	self->len = 0;
	self->cap = 0;
}

Object *list_get_attr(Object *_self, Object *name) {
	ListObject *self = (ListObject*)_self;
	if (object_equals_str(name, "len")) {
		return (Object*)int_raw(self->len);
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *builtinfunction_call(Object *_self, TupleObject *args) {
	BuiltinFunctionObject *self = (BuiltinFunctionObject*)_self;
	return self->func(args);
}

Object *type_constructor(Object *self, TupleObject *args) {
	if (args->len == 1) {
		return (Object*)args->data[0]->type;
	} else if (args->len == 2) {
		// OH BOY
		// first arg (base) must be a TypeObject
		// second arg (dict) must be a dict
		Object *_arg1 = args->data[0];
		if (!isinstance_inner(_arg1, &g_type)) {
			error = exc_msg(&g_TypeError, "Argument 1: expected type");
			return NULL;
		}
		Object *_arg2 = args->data[1];
		if (!isinstance_inner(_arg2, &g_dict)) {
			error = exc_msg(&g_TypeError, "Argument 2: expected dict");
			return NULL;
		}
		DictObject *arg2 = (DictObject*)_arg2;
		TypeObject *result = (TypeObject*)gc_alloc(sizeof(TypeObject));
		result->header_basic.header_dict.header.table = &type_table;
		result->header_basic.header_dict.header.type = (TypeObject*)self;
		result->base_class = (TypeObject*)_arg1,
		result->constructor = result->base_class->constructor;
		bool inner_tracer(void *key, void **val) {
			return dict_set(&result->header_basic.header_dict.core, key, *val, object_hasher, object_equals, current_thread_alloc, current_thread_dealloc);
		}
		dict_trace(&arg2->core, inner_tracer);
		return (Object*)result;
	} else {
		error = exc_msg(&g_TypeError, "Expected 1 or 2 arguments");
		return NULL;
	}
}

size_t type_size(Object *_self) {
	TypeObject *self = (TypeObject*)_self;
	return sizeof(TypeObject) + dict_size(&self->header_basic.header_dict.core);
}

bool type_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	TypeObject *self = (TypeObject*)_self;
	if (!object_trace((Object*)&self->header_basic, tracer)) return false;
	if (self->base_class) {
		if (!tracer((Object*)self->base_class)) return false;
	}
	return true;
}

Object *type_get_attr(Object *self, Object *name) {
	// some hacks :)
	if (self == (Object*)&g_bytes && name->type == &g_bytes) {
		if (strncmp(bytes_data((BytesObject*)name), "__hash__", strlen("__hash__")) == 0) {
			return (Object*)&g_bytes___hash__;
		}
		if (strncmp(bytes_data((BytesObject*)name), "__eq__", strlen("__eq__")) == 0) {
			return (Object*)&g_bytes___eq__;
		}
	}
	return object_get_attr(self, name);
}

Object *type_call(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject*)_self;
	return self->constructor(_self, args);
}

Object *closure_constructor(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject*)_self;
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bytes)) {
		error = exc_msg(&g_TypeError, "Argument 1: expected bytes");
		return NULL;
	}
	if (!isinstance_inner(args->data[1], &g_dict)) {
		error = exc_msg(&g_TypeError, "Argument 2: expected dict");
		return NULL;
	}

	return (Object*)closure_raw_ex((BytesObject*)args->data[0], (DictObject*)args->data[1], self);
}

bool closure_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	ClosureObject *self = (ClosureObject*)_self;
	if (!tracer((Object*)self->bytecode)) return false;
	if (!tracer((Object*)self->context)) return false;
	return true;
}

Object *closure_get_attr(Object *_self, Object *name) {
	ClosureObject *self = (ClosureObject*)_self;
	if (object_equals_str(name, "code")) {
		return (Object*)self->bytecode;
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *closure_call(Object *_self, TupleObject *args) {
	if (!isinstance_inner(_self, &g_closure)) {
		error = exc_msg(&g_TypeError, "how did you do that");
		return NULL;
	}
	ClosureObject *self = (ClosureObject*)_self;
	return interpreter(self, args);
}

Object *bytes_constructor(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject*)_self;
	if (args->len == 0) {
		return (Object*)bytes_raw_ex("", 0, self);
	}
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 arguments");
		return NULL;
	}
	if (isinstance_inner(args->data[0], &g_bytes) || isinstance_inner(args->data[0], &g_bytearray)) {
		BytesObject *arg = (BytesObject*)args->data[0];
		return (Object*)bytes_raw_ex(bytes_data(arg), arg->len, self);
	}
	Object *method = get_attr_inner(args->data[0], "__str__");
	if (method == NULL) {
		return NULL;
	}
	TupleObject *new_args = tuple_raw(NULL, 0);
	gc_root((Object*)new_args);
	Object *result = call(method, new_args);
	gc_unroot((Object*)new_args);
	if (result == NULL) {
		return NULL;
	}
	if (!isinstance_inner(result, &g_bytes)) {
		error = exc_msg(&g_TypeError, "__str__ did not return a bytestring");
		return NULL;
	}
	if (result->type != self) {
		BytesObject *arg = (BytesObject*)result;
		return (Object*)bytes_raw_ex(bytes_data(arg), arg->len, self);
	}
	return result;
}

BytesObject *bytes_constructor_inner(Object *arg) {
	TupleObject *args = tuple_raw(&arg, 1);
	if (args == NULL) {
		return NULL;
	}
	gc_root((Object*)args);
	Object *result = bytes_constructor((Object*)&g_bytes, args);
	gc_unroot((Object*)args);
	return (BytesObject*)result;
}

size_t bytes_size(Object *_self) {
	BytesObject *self = (BytesObject*)_self;
	return sizeof(BytesObject) + self->len * sizeof(char);
}

const char *bytes_data(BytesObject *self) {
	if (self->header.table == &bytes_unowned_table) {
		return ((BytesUnownedObject*)self)->_data;
	} else if (self->header.table == &bytearray_table) {
		return ((BytearrayObject*)self)->data;
	} else {
		return self->_data;
	}
}

Object *bytes_get_attr(Object *_self, Object *name) {
	BytesObject *self = (BytesObject*)_self;
	if (object_equals_str(name, "len")) {
		return (Object*)int_raw(self->len);
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *bytearray_constructor(Object *self, TupleObject *args) {
	if (args->len == 0) {
		return (Object*)bytearray_raw(NULL, 0, (TypeObject*)self);
	}
	if (args->len == 1 && isinstance_inner(args->data[0], &g_int)) {
		uint64_t size = ((IntObject*)args->data[0])->value;
		BytearrayObject *result = bytearray_raw(NULL, size, (TypeObject*)self);
		if (result == NULL) {
			return NULL;
		}
		memset(result->data, 0, size);
		return (Object*)result;
	} else if (args->len == 1 && (isinstance_inner(args->data[0], &g_bytes) || isinstance_inner(args->data[0], &g_bytearray))) {
		BytesObject* arg = (BytesObject*)args->data[0];
		return (Object*)bytearray_raw(bytes_data(arg), arg->len, (TypeObject*)self);
	} else {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 arguments, argument 1 must be int, bytes, or bytearray");
		return NULL;
	}
}

void bytearray_finalize(Object *_self) {
	BytearrayObject *self = (BytearrayObject*)_self;
	quota_dealloc(self->data, self->cap, self->header_bytes.header.group);
	self->data = NULL;
	self->header_bytes.len = 0;
	self->cap = 0;
}

size_t bytearray_size(Object *_self) {
	BytearrayObject *self = (BytearrayObject*)_self;
	return sizeof(BytearrayObject) + self->cap;
}

Object *bool_constructor(Object *self, TupleObject *args) {
	if (args->len == 0) {
		return (Object*)&g_false;
	}
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 arguments");
		return NULL;
	}
	Object *method = get_attr_inner(args->data[0], "__bool__");
	if (method == NULL) {
		return NULL;
	}
	TupleObject *new_args = tuple_raw(NULL, 0);
	gc_root((Object*)new_args);
	Object *result = call(method, new_args);
	gc_unroot((Object*)new_args);
	if (result == NULL) {
		return NULL;
	}
	if (result->type != &g_bool) {
		error = exc_msg(&g_TypeError, "__bool__ did not return a bool");
		return NULL;
	}
	return result;
}

EmptyObject *bool_constructor_inner(Object *obj) {
	TupleObject *inner_args = tuple_raw(&obj, 1);
	if (inner_args == NULL) {
		return NULL;
	}
	gc_root((Object*)inner_args);
	Object *result = bool_constructor((Object*)&g_bool, inner_args);
	gc_unroot((Object*)inner_args);
	return (EmptyObject*)result;
}

Object *dict_constructor(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject*)_self;
	if (args->len == 0) {
		return (Object*)dicto_raw_ex(self);
	} else if (args->len == 1) {
		if (!isinstance_inner(args->data[0], &g_dict)) {
			error = exc_msg(&g_TypeError, "Expected dict");
			return NULL;
		}
		DictObject *input = (DictObject*)args->data[0];
		DictObject *result = dicto_raw_ex(self);
		gc_root((Object*)result);
		bool inner_tracer(void *key, void **val) {
			return dict_set(&result->core, key, *val, object_hasher, object_equals, current_thread_alloc, current_thread_dealloc);
		}
		if (!dict_trace(&input->core, inner_tracer)) {
			gc_unroot((Object*)result);
			return NULL;
		}
		gc_unroot((Object*)result);
		return (Object*)result;
	}
	error = exc_msg(&g_TypeError, "Expected 0 or 1 arguments");
	return NULL;
}

DictObject *dict_dup_inner(DictObject *self) {
	TupleObject *args = tuple_raw((Object**)&self, 1);
	gc_root((Object*)args);
	Object *result = dict_constructor((Object*)&g_dict, args);
	gc_unroot((Object*)args);
	return (DictObject*)result;
}

Object *nonetype_constructor(Object *self, TupleObject *args) {
	if (args->len != 0) {
		error = exc_msg(&g_TypeError, "Expected 0 arguments");
		return NULL;
	}
	return (Object*)&g_none;
}

Object *null_constructor(Object *self, TupleObject *args) {
	error = exc_msg(&g_RuntimeError, "Object cannot be constructed");
	return NULL;
}

bool boundmeth_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	BoundMethodObject *self = (BoundMethodObject*)_self;
	if (!tracer((Object*)self->method)) return false;
	if (!tracer((Object*)self->self)) return false;
	return true;
}

Object *boundmeth_get_attr(Object *_self, Object *name) {
	BoundMethodObject *self = (BoundMethodObject*)_self;
	if (object_equals_str(name, "method")) {
		return (Object*)self->method;
	}
	if (object_equals_str(name, "self")) {
		return (Object*)self->self;
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *boundmeth_call(Object *_self, TupleObject *args) {
	BoundMethodObject *self = (BoundMethodObject*)_self;
	Object **stuff = alloca(sizeof(Object*) * (args->len + 1));
	stuff[0] = self->self;
	memcpy(&stuff[1], args->data, sizeof(Object*) * args->len);
	TupleObject *new_args = tuple_raw(stuff, args->len + 1);
	if (new_args == NULL) {
		return NULL;
	}
	gc_root((Object*)new_args);
	Object *result = call(self->method, new_args);
	gc_unroot((Object*)new_args);
	return result;
}

bool bytes_unowned_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	BytesUnownedObject *self = (BytesUnownedObject*)_self;
	if (self->owner == NULL) {
		// interned string
		return true;
	}
	return tracer(self->owner);
}

Object *bytes_unowned_get_attr(Object *_self, Object *name) {
	BytesUnownedObject *self = (BytesUnownedObject*)_self;
	if (object_equals_str(name, "len")) {
		return (Object*)int_raw(self->header_bytes.len);
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *slice_constructor(Object *self, TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	return (Object*)slice_raw(args->data[0], args->data[1]);
}

bool slice_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	SliceObject *self = (SliceObject*)_self;
	if (!tracer(self->start)) return false;
	if (!tracer(self->end)) return false;
	return true;
}

Object *slice_get_attr(Object *_self, Object *name) {
	SliceObject *self = (SliceObject*)_self;
	if (object_equals_str(name, "start")) {
		return self->start;
	}
	if (object_equals_str(name, "end")) {
		return self->end;
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *exc_constructor(Object *self, TupleObject *args) {
	return (Object*)exc_raw((TypeObject*)self, args);
}

bool exc_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	ExceptionObject *self = (ExceptionObject*)_self;
	if (!tracer((Object*)self->args)) return false;
	return true;
}

Object *exc_get_attr(Object *_self, Object *name) {
	ExceptionObject *self = (ExceptionObject*)_self;
	if (object_equals_str(name, "args")) {
		return (Object*)self->args;
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *get_attr_inner(Object *self, char *name) {
	Object *temp = (Object*)bytes_raw(name, strlen(name));
	if (!temp) {
		return NULL;
	}
	gc_root(temp);
	Object *result = get_attr(self, temp);
	gc_unroot(temp);
	return result;
}
Object *get_attr(Object *self, Object *name) {
	Object *result;
	bool check_own = true;

	// is there a better fucking way to do tihs??????
	if (name->type == &g_bytes && ((BytesObject*)name)->len >= 2 && bytes_data((BytesObject*)name)[0] == '_' && bytes_data((BytesObject*)name)[1] == '_') {
		check_own = false;
	}

	if (check_own) {
		result = self->table->get_attr(self, name);
		if (result != NULL) {
			return result;
		}
		error = NULL;

		// is this right?
		if (self->table == &type_table) {
			for (TypeObject *type = ((TypeObject*)self)->base_class; type; type = type->base_class) {
				result = type->header_basic.header_dict.header.table->get_attr((Object*)type, name);
				if (result != NULL) {
					return result;
				}
				error = NULL;
			}
		}
	}
	
	for (TypeObject *type = self->type; type; type = type->base_class) {
		result = type->header_basic.header_dict.header.table->get_attr((Object*)type, name);
		if (result != NULL) {
			// maybe could be an interesting source of bugs if we let any object be bound and do property forwards through boundmeth
			if (isinstance_inner(result, &g_builtin) || isinstance_inner(result, &g_closure)) {
				result = (Object*)boundmeth_raw(result, self);
			}
			return result;
		}
		error = NULL;
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}
bool set_attr_inner(Object *self, char *name, Object *value) {
	Object *temp = (Object*)bytes_raw(name, strlen(name));
	if (!temp) {
		return NULL;
	}
	gc_root(temp);
	bool result = set_attr(self, temp, value);
	gc_unroot(temp);
	return result;
}
bool set_attr(Object *self, Object *name, Object *value) {
	return self->table->set_attr(self, name, value);
}

bool del_attr_inner(Object *self, char *name) {
	Object *temp = (Object*)bytes_raw(name, strlen(name));
	if (!temp) {
		return NULL;
	}
	gc_root(temp);
	bool result = del_attr(self, temp);
	gc_unroot(temp);
	return result;
}
bool del_attr(Object *self, Object *name) {
	return self->table->del_attr(self, name);
}

Object *call(Object *method, TupleObject *args) {
	return method->table->call(method, args);
}

bool trace(Object *self, bool (*tracer)(Object *tracee)) {
	if (!tracer((Object*)self->type)) return false;
	if (self->group != NULL) {
		// static objects have no threadgroup since they do not count toward any quota
		if (!tracer((Object*)self->group)) return false;
	}
	return self->table->trace(self, tracer);
}

size_t size(Object *self) {
	if (self->table->size.given < 0x1000) {
		return self->table->size.given;
	} else {
		return self->table->size.computed(self);
	}
}

HashResult object_hasher(void *val) {
	Object *self = (Object*)val;
	Object *method = get_attr_inner(self, "__hash__");
	if (!method) {
		return (HashResult) {
			.hash = 0,
			.success = false,
		};
	}
	TupleObject *args = tuple_raw(NULL, 0);
	gc_root((Object*)args);
	Object *result = call(method, args);
	gc_unroot((Object*)args);
	if (result == NULL) {
		return (HashResult) {
			.hash = 0,
			.success = false,
		};
	}
	if (!isinstance_inner(result, &g_int)) {
		error = exc_msg(&g_TypeError, "__hash__ did not return an int");
		return (HashResult) {
			.hash = 0,
			.success = false,
		};
	}
	return (HashResult) {
		.hash = ((IntObject*)result)->value,
		.success = true,
	};
}

EqualityResult object_equals(void *_val1, void *_val2) {
	Object *val1 = (Object*)_val1;
	Object *val2 = (Object*)_val2;
	Object *method = get_attr_inner(val1, "__eq__");
	if (!method) {
		return (EqualityResult) {
			.equals = false,
			.success = false,
		};
	}
	TupleObject *args = tuple_raw(&val2, 1);
	gc_root((Object*)args);
	Object *result = call(method, args);
	gc_unroot((Object*)args);
	if (result == NULL) {
		return (EqualityResult) {
			.equals = false,
			.success = false,
		};
	}
	if (!isinstance_inner(result, &g_bool)) {
		error = exc_msg(&g_TypeError, "__eq__ did not return a bool");
		return (EqualityResult) {
			.equals = false,
			.success = false,
		};
	}
	return (EqualityResult) {
		.equals = result == (Object*)&g_true,
		.success = true,
	};
}

extern MemberInitEntry __start_static_member_adds;
extern MemberInitEntry __stop_static_member_adds;
__attribute__((constructor)) void init_static_adds() {
	for (MemberInitEntry *iter = &__start_static_member_adds; iter != &__stop_static_member_adds; iter++) {
		if (!dict_set(&iter->self->core, (void*)iter->name, (void*)iter->value, object_hasher, object_equals, global_alloc, global_dealloc)) {
			puts("Fatal error");
			exit(1);
		}
	}
}
