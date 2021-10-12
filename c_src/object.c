#include <string.h>
#include <stdio.h>
#include <alloca.h>

#include "object.h"
#include "gc.h"
#include "errors.h"
#include "interpreter.h"

// tables and instances
ObjectTable null_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = null_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};
ObjectTable dicto_table = {
	.trace = dicto_trace,
	.finalize = dicto_finalize,
	.get_attr = dicto_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};
ObjectTable object_table = {
	.trace = object_trace,
	.finalize = object_finalize,
	.get_attr = object_get_attr,
	.set_attr = object_set_attr,
	.del_attr = object_del_attr,
	.call = object_call,
};
ObjectTable type_table = {
	.trace = type_trace,
	.finalize = null_finalize,
	.get_attr = type_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = type_call,
};
ObjectTable int_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = null_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};
ObjectTable float_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = null_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};
ObjectTable list_table = {
	.trace = list_trace,
	.finalize = list_finalize,
	.get_attr = list_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};
ObjectTable tuple_table = {
	.trace = tuple_trace,
	.finalize = null_finalize,
	.get_attr = tuple_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};
ObjectTable bytes_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = bytes_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};
ObjectTable bytes_unowned_table = {
	.trace = bytes_unowned_trace,
	.finalize = null_finalize,
	.get_attr = bytes_unowned_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};
ObjectTable builtinfunction_table = {
	.trace = null_trace,
	.finalize = null_finalize,
	.get_attr = null_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = builtinfunction_call,
};
ObjectTable closure_table = {
	.trace = closure_trace,
	.finalize = null_finalize,
	.get_attr = closure_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = closure_call,
};
ObjectTable boundmeth_table = {
	.trace = boundmeth_trace,
	.finalize = null_finalize,
	.get_attr = boundmeth_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = boundmeth_call,
};
ObjectTable slice_table = {
	.trace = slice_trace,
	.finalize = null_finalize,
	.get_attr = slice_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};
ObjectTable exc_table = {
	.trace = exc_trace,
	.finalize = null_finalize,
	.get_attr = exc_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};

// builtin type class instances


Object *object_constructor(Object *self, TupleObject *args);
INTERNED_STRING(g_object_name, "object");
TypeObject g_object = {
	.header_basic.header_dict.header = {
		.table = &type_table,
		.type = &g_type,
	},
	.base_class = NULL,
	.constructor = object_constructor,
	.name = (BytesObject*)&g_object_name,
};
STATIC_OBJECT(g_object);
ADD_MEMBER(builtins, "object", g_object);

BUILTIN_TYPE(type, object, type_constructor);
BUILTIN_TYPE(nonetype, object, nonetype_constructor);
BUILTIN_TYPE(int, object, int_constructor);
BUILTIN_TYPE(float, object, float_constructor);
BUILTIN_TYPE(bool, object, bool_constructor);
BUILTIN_TYPE(bytes, object, bytes_constructor);
BUILTIN_TYPE(dict, object, dict_constructor);
BUILTIN_TYPE(list, object, list_constructor);
BUILTIN_TYPE(tuple, object, tuple_constructor);
BUILTIN_TYPE(builtin, object, builtin_constructor);
BUILTIN_TYPE(closure, object, closure_constructor);
BUILTIN_TYPE(boundmeth, object, boundmeth_constructor);
BUILTIN_TYPE(slice, object, slice_constructor);
BUILTIN_TYPE(exception, object, exc_constructor);

EmptyObject g_none = {
	.header = (ObjectHeader) {
		.type = &g_nonetype,
		.table = &null_table,
	},
};
STATIC_OBJECT(g_none);
EmptyObject g_true = {
	.header = (ObjectHeader) {
		.type = &g_bool,
		.table = &null_table,
	},
};
STATIC_OBJECT(g_true);
EmptyObject g_false = {
	.header = (ObjectHeader) {
		.type = &g_bool,
		.table = &null_table,
	},
};
STATIC_OBJECT(g_false);
TupleObject empty_tuple = {
	.header = (ObjectHeader) {
		.type = &g_tuple,
		.table = &tuple_table,
	},
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
	IntObject *result = (IntObject *)gc_malloc(sizeof(IntObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.table = &int_table,
		.type = &g_int,
	};
	result->value = value;
	return result;
}

EmptyObject *bool_raw(bool value) {
	return value ? &g_true : &g_false;
}

FloatObject *float_raw(double value) {
	FloatObject *result = (FloatObject*)gc_malloc(sizeof(IntObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.table = &float_table,
		.type = &g_float,
	};
	result->value = value;
	return result;
}

ListObject *list_raw(Object **data, size_t len) {
	ListObject *result = (ListObject*)gc_malloc(sizeof(ListObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.type = &g_list,
		.table = &list_table,
	};
	result->len = len;
	result->cap = len;
	result->data = malloc(sizeof(Object*) * len);
	if (!result->data) {
		error = (Object*)&MemoryError_inst;
		free(result);
		return NULL;
	}
	memcpy(result->data, data, sizeof(Object*) * len);
	return result;
}

TupleObject *tuple_raw(Object **data, size_t len) {
	if (len == 0) {
		return &empty_tuple;
	}
	TupleObject *result = (TupleObject*)gc_malloc(sizeof(TupleObject) + sizeof(Object*) * len);
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.type = &g_tuple,
		.table = &tuple_table,
	};
	result->len = len;
	memcpy(result->data, data, sizeof(Object*) * len);
	return result;
}

BytesObject *bytes_raw(const char *data, size_t len) {
	BytesObject *result = (BytesObject*)gc_malloc(sizeof(BytesObject) + len * sizeof(char));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.type = &g_bytes,
		.table = &bytes_table,
	};
	result->len = len;
	memcpy(result->_data, data, len * sizeof(char));
	return result;
}

BytesUnownedObject *bytes_unowned_raw(const char *data, size_t len, Object *owner) {
	BytesUnownedObject *result = (BytesUnownedObject*)gc_malloc(sizeof(BytesUnownedObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}

	result->header_bytes.header = (ObjectHeader) {
		.type = &g_bytes,
		.table = &bytes_unowned_table,
	};
	result->header_bytes.len = len;
	result->_data = data;
	result->owner = owner;
	return result;
}

DictObject *dicto_raw() {
	DictObject *result = (DictObject *)gc_malloc(sizeof(DictObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.type = &g_dict,
		.table = &dicto_table
	};
	return result;
}

BasicObject *object_raw(TypeObject *type) {
	BasicObject *result = (BasicObject*)gc_malloc(sizeof(BasicObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header_dict.header = (ObjectHeader) {
		.type = type,
		.table = &object_table,
	};
	return result;
}

ClosureObject *closure_raw(BytesObject *name, BytesObject *bytecode, DictObject *context) {
	ClosureObject *result = (ClosureObject*)gc_malloc(sizeof(ClosureObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.type = &g_closure,
		.table = &closure_table,
	};
	result->name = name;
	result->bytecode = bytecode;
	result->context = context;
	return result;
}

BoundMethodObject *boundmeth_raw(Object *meth, Object *self) {
	BoundMethodObject *result = (BoundMethodObject*)gc_malloc(sizeof(BoundMethodObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.type = &g_boundmeth,
		.table = &boundmeth_table,
	};
	result->method = meth;
	result->self = self;
	return result;
}

SliceObject *slice_raw(Object *start, Object *end) {
	SliceObject *result = (SliceObject*)gc_malloc(sizeof(SliceObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.type = &g_slice,
		.table = &slice_table,
	};
	result->start = start;
	result->end = end;
	return result;
}

ExceptionObject *exc_raw(TypeObject *type, TupleObject *args) {
	ExceptionObject *result = (ExceptionObject*)gc_malloc(sizeof(ExceptionObject));
	if (!result) {
		error = (Object*)&MemoryError_inst;
		return NULL;
	}
	result->header = (ObjectHeader) {
		.type = type,
		.table = &exc_table,
	};
	result->args = args;
	return result;
}

// MAKE SURE YOU FREE THE RESULT OF THIS! IT IS NOT GARBAGE COLLECTED!
char *c_str(Object *arg) {
	if (arg->type != &g_bytes) {
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

IntObject *bytes2int(BytesObject *arg, int64_t base) {
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
	return int_raw(value);
}

FloatObject *bytes2float(BytesObject *arg) {
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
	return float_raw(value);
}

Object* int_constructor(Object *self, TupleObject *args) {
	if (args->len == 0) {
		return (Object*)int_raw(0);
	} else if (args->len == 1) {
		Object *arg = args->data[0];
		if (arg->type == &g_int) {
			return arg;
		} else if (arg->type == &g_float) {
			return (Object*)int_raw((int64_t)((FloatObject*)arg)->value);
		} else if (arg->type == &g_bytes) {
			return (Object*)bytes2int((BytesObject*)arg, 10);
		} else {
			error = exc_msg(&g_TypeError, "Expected int, float, or bytes");
			return NULL;
		}
	} else if (args->len == 2) {
		Object *arg0 = args->data[0];
		Object *arg1 = args->data[1];
		if (arg0->type == &g_bytes && arg1->type == &g_int) {
			return (Object*)bytes2int((BytesObject*)arg0, ((IntObject*)arg1)->value);
		} else {
			error = exc_msg(&g_TypeError, "Expected bytes and int");
			return NULL;
		}
	} else {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 or 2 arguments");
		return NULL;
	}
}

Object *float_constructor(Object *self, TupleObject *args) {
	if (args->len == 0) {
		return (Object*)float_raw(0.);
	} else if (args->len == 1) {
		Object *arg = args->data[0];
		if (arg->type == &g_float) {
			return arg;
		} else if (arg->type == &g_int) {
			return (Object*)float_raw((double)((IntObject*)arg)->value);
		} else if (arg->type == &g_bytes) {
			return (Object*)bytes2float((BytesObject*)arg);
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
	dict_destruct(&self->header_dict.core);
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
	return dict_set(&self->header_dict.core, (void*)name, (void*)val, object_hasher, object_equals);
}

bool object_del_attr(Object *_self, Object *name) {
	BasicObject *self = (BasicObject*)_self;
	GetResult result = dict_pop(&self->header_dict.core, (void*)name, object_hasher, object_equals);
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
	dict_destruct(&self->core);
}

Object *dicto_get_attr(Object *_self, Object *name) {
	DictObject *self = (DictObject*)_self;
	if (object_equals_str(name, "len")) {
		return (Object*)int_raw(self->core.len);
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *tuple_constructor(Object *self, TupleObject *args) {
	if (args->len == 0) {
		return (Object*)tuple_raw(NULL, 0);
	} else if (args->len == 1) {
		Object *arg = args->data[0];
		if (arg->type == &g_tuple) {
			return arg;
		} else if (arg->type == &g_list) {
			return (Object*)tuple_raw(((ListObject*)arg)->data, ((ListObject*)arg)->len);
		} else {
			error = exc_msg(&g_TypeError, "Expected tuple or list");
			return NULL;
		}
	} else {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 args");
		return NULL;
	}
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

Object *list_constructor(Object *self, TupleObject *args) {
	if (args->len == 0) {
		return (Object*)list_raw(NULL, 0);
	} else if (args->len == 1) {
		Object *arg = args->data[0];
		if (arg->type == &g_tuple) {
			return (Object*)list_raw(((TupleObject*)arg)->data, ((TupleObject*)arg)->len);
		} else if (arg->type == &g_list) {
			return (Object*)list_raw(((ListObject*)arg)->data, ((ListObject*)arg)->len);
		} else {
			error = exc_msg(&g_TypeError, "Expected tuple or list");
			return NULL;
		}
	} else {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 args");
		return NULL;
	}
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
	free(self->data);
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

Object *type_constructor(Object *_self, TupleObject *args) {
	if (args->len == 1) {
		return (Object*)args->data[0]->type;
	} else if (args->len == 3) {
		// OH BOY
		// first arg (name) must be a string
		// second arg (base) must be a TypeObject
		// third arg (dict) must be a dict
		Object *_arg0 = args->data[0];
		if (_arg0->type != &g_bytes) {
			error = exc_msg(&g_TypeError, "Argument 1: expected bytes");
			return NULL;
		}
		Object *_arg1 = args->data[1];
		if (_arg1->type != &g_type) {
			error = exc_msg(&g_TypeError, "Argument 2: expected type");
			return NULL;
		}
		Object *_arg2 = args->data[2];
		if (_arg1->type != &g_dict) {
			error = exc_msg(&g_TypeError, "Argument 3: expected dict");
			return NULL;
		}
		DictObject *arg2 = (DictObject*)_arg2;
		TypeObject *result = (TypeObject*)gc_malloc(sizeof(TypeObject));
		result->header_basic.header_dict.header = (ObjectHeader) {
			.table = &type_table,
			.type = &g_type,
		};
		result->base_class = (TypeObject*)_arg1,
		result->constructor = result->base_class->constructor;
		result->name = (BytesObject*)_arg0;
		bool inner_tracer(void *key, void **val) {
			return dict_set(&result->header_basic.header_dict.core, key, *val, object_hasher, object_equals);
		}
		dict_trace(&arg2->core, inner_tracer);
		return (Object*)result;
	} else {
		error = exc_msg(&g_TypeError, "Expected 1 or 3 arguments");
		return NULL;
	}
}

bool type_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	TypeObject *self = (TypeObject*)_self;
	if (!object_trace((Object*)&self->header_basic, tracer)) return false;
	if (self->base_class) {
		if (!tracer((Object*)self->base_class)) return false;
	}
	if (!tracer((Object*)self->name)) return false;
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

Object *closure_constructor(Object *self, TupleObject *args) {
	if (args->len != 3) {
		error = exc_msg(&g_TypeError, "Expected 3 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_bytes) {
		error = exc_msg(&g_TypeError, "Argument 1: expected bytes");
		return NULL;
	}
	if (args->data[1]->type != &g_bytes) {
		error = exc_msg(&g_TypeError, "Argument 2: expected bytes");
		return NULL;
	}
	if (args->data[2]->type != &g_dict) {
		error = exc_msg(&g_TypeError, "Argument 3: expected dict");
		return NULL;
	}

	return (Object*)closure_raw((BytesObject*)args->data[0], (BytesObject*)args->data[1], (DictObject*)args->data[2]);
}

bool closure_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	ClosureObject *self = (ClosureObject*)_self;
	if (!tracer((Object*)self->name)) return false;
	if (!tracer((Object*)self->bytecode)) return false;
	if (!tracer((Object*)self->context)) return false;
	return true;
}

Object *closure_get_attr(Object *_self, Object *name) {
	ClosureObject *self = (ClosureObject*)_self;
	if (object_equals_str(name, "name")) {
		return (Object*)self->name;
	}
	if (object_equals_str(name, "code")) {
		return (Object*)self->bytecode;
	}

	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object *closure_call(Object *_self, TupleObject *args) {
	if (_self->type != &g_closure) {
		error = exc_msg(&g_TypeError, "how did you do that");
		return NULL;
	}
	ClosureObject *self = (ClosureObject*)_self;
	return interpreter(self, args);
}

Object *bytes_constructor(Object *self, TupleObject *args) {
	if (args->len == 0) {
		return (Object*)bytes_raw("", 0);
	}
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 arguments");
		return NULL;
	}
	Object *method = get_attr_inner(args->data[0], "__str__");
	if (method == NULL) {
		return NULL;
	}
	TupleObject *new_args = tuple_raw(NULL, 0);
	gc_root((Object*)new_args);
	Object *result = call(method, new_args);
	gc_unroot((Object*)new_args);
	// if we're strapped for bugs we can remove this check
	if (result->type != &g_bytes) {
		error = exc_msg(&g_TypeError, "__str__ did not return a bytestring");
		return NULL;
	}
	return result;
}

BytesObject *bytes_constructor_inner(Object *arg) {
	TupleObject *args = tuple_raw(&arg, 1);
	if (args == NULL) {
		return NULL;
	}
	gc_root((Object*)args);
	Object *result = bytes_constructor(NULL, args);
	gc_unroot((Object*)args);
	return (BytesObject*)result;
}

const char *bytes_data(BytesObject *self) {
	if (self->header.table == &bytes_unowned_table) {
		return ((BytesUnownedObject*)self)->_data;
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

Object *bool_constructor(Object *self, TupleObject *args) {
	if (args->len == 0) {
		return (Object*)&g_false;
	}
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 0 or 1 arguments");
		return NULL;
	}
	Object *method = get_attr_inner(self, "__bool__");
	if (method == NULL) {
		return NULL;
	}
	TupleObject *new_args = tuple_raw(NULL, 0);
	gc_root((Object*)new_args);
	Object *result = call(method, new_args);
	gc_unroot((Object*)new_args);
	// if we're strapped for bugs we can remove this check
	if (result->type != &g_bool) {
		error = exc_msg(&g_TypeError, "__bool__ did not return a bool");
		return NULL;
	}
	return result;
}

Object *builtin_constructor(Object *self, TupleObject *args) {
	error = exc_msg(&g_RuntimeError, "how did you do that");
	return NULL;
}

Object *dict_constructor(Object *self, TupleObject *args) {
	if (args->len == 0) {
		return (Object*)dicto_raw();
	} else if (args->len == 1) {
		if (args->data[0]->type != &g_dict) {
			error = exc_msg(&g_TypeError, "Expected dict");
			return NULL;
		}
		DictObject *input = (DictObject*)args->data[0];
		DictObject *result = dicto_raw();
		gc_root((Object*)result);
		bool inner_tracer(void *key, void **val) {
			return dict_set(&result->core, key, *val, object_hasher, object_equals);
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
	Object *result = dict_constructor(NULL, args);
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

Object *boundmeth_constructor(Object *self, TupleObject *args) {
	error = exc_msg(&g_RuntimeError, "how did you do that");
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
	
	for (TypeObject *type = self->type; type; type = type->base_class) {
		result = type->header_basic.header_dict.header.table->get_attr((Object*)type, name);
		if (result != NULL) {
			// maybe could be an interesting source of bugs if we let any object be bound and do property forwards through boundmeth
			if (result->type == &g_builtin || result->type == &g_closure) {
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
	if (!tracer((Object*)self->type)) {
		return false;
	}
	return self->table->trace(self, tracer);
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
	if (result->type != &g_int) {
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
	if (result->type != &g_bool) {
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
		BytesUnownedObject *name = bytes_unowned_raw(iter->name, strlen(iter->name), NULL);
		if (name == NULL) {
			puts("Fatal error");
			exit(1);
		}
		if (!dict_set(&iter->self->core, (void*)name, (void*)iter->value, object_hasher, object_equals)) {
			puts("Fatal error");
			exit(1);
		}
	}
}
