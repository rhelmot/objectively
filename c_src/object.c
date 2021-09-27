#include <string.h>
#include <stdio.h>
#include <alloca.h>

#include "object.h"
#include "gc.h"

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

// builtin type class instances

#define BUILTIN_TYPE(name, base, cons_func) \
Object *(cons_func)(Object *self, TupleObject *args); \
TypeObject g_##name = { \
	.header_basic.header_dict.header = { \
		.table = &type_table, \
		.type = &g_type, \
	}, \
	.base_class = &g_##base, \
	.constructor = (cons_func), \
}; \
STATIC_OBJECT(g_##name);


Object *object_constructor(Object *self, TupleObject *args);
TypeObject g_object = {
	.header_basic.header_dict.header = {
		.table = &type_table,
		.type = &g_type,
	},
	.base_class = NULL,
	.constructor = object_constructor,
};
STATIC_OBJECT(g_object);


BUILTIN_TYPE(type, object, type_constructor);
BUILTIN_TYPE(nonetype, object, nonetype_constructor);
BUILTIN_TYPE(int, object, int_constructor);
BUILTIN_TYPE(float, object, float_constructor);
BUILTIN_TYPE(bool, int, bool_constructor);
BUILTIN_TYPE(bytes, object, bytes_constructor);
BUILTIN_TYPE(dict, object, dict_constructor);
BUILTIN_TYPE(list, object, list_constructor);
BUILTIN_TYPE(tuple, object, tuple_constructor);
BUILTIN_TYPE(builtin, object, builtin_constructor);
BUILTIN_TYPE(closure, object, closure_constructor);
BUILTIN_TYPE(boundmeth, object, boundmeth_constructor);
BUILTIN_TYPE(slice, object, slice_constructor);

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

bool null_trace(Object *self, bool (*tracer)(Object *tracee)) {
	return true;
}
void null_finalize(Object *self) {
	return;
}
Object *null_get_attr(Object *self, Object *name) {
	// TODO set attributeerror
	return NULL;
}
bool null_set_attr(Object *self, Object *name, Object *value) {
	// TODO set attributeerror
	return NULL;
}
bool null_del_attr(Object *self, Object *name) {
	// TODO set attributeerror
	return NULL;
}
Object *null_call(Object *self, TupleObject *args) {
	// TODO set typeerror
	return NULL;
}

IntObject *int_raw(int64_t value) {
	IntObject *result = (IntObject *)gc_malloc(sizeof(IntObject));
	if (!result) {
		// TODO set memoryerror
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
		// TODO set memoryerror
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
		// TODO set memoryerror
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
		// TODO set memoryerror
		free(result);
		return NULL;
	}
	memcpy(result->data, data, sizeof(Object*) * len);
	return result;
}

TupleObject *tuple_raw(Object **data, size_t len) {
	TupleObject *result = (TupleObject*)gc_malloc(sizeof(TupleObject) + sizeof(Object*) * len);
	if (!result) {
		// TODO set memoryerror
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
		// TODO set memoryerror
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

BytesUnownedObject *bytes_unowned_raw(char *data, size_t len, Object *owner) {
	BytesUnownedObject *result = (BytesUnownedObject*)gc_malloc(sizeof(BytesUnownedObject));
	if (!result) {
		// TODO set memoryerror
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
		// TODO set memoryerror
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
		// TODO set memoryerror
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
		// TODO set memoryerror
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
		// TODO set memoryerror
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
		// TODO set memoryerror
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

// MAKE SURE YOU FREE THE RESULT OF THIS! IT IS NOT GARBAGE COLLECTED!
char *c_str(Object *arg) {
	if (arg->type != &g_bytes) {
		// TODO set typeerror
		return NULL;
	}
	BytesObject *str = (BytesObject*)arg;
	if (memchr(bytes_data(str), '\0', str->len) != NULL) {
		// TODO set valueerror
		return NULL;
	}
	return strndup(bytes_data(str), str->len);
}

IntObject *bytes2int(BytesObject *arg, int64_t base) {
	char *buffer = c_str((Object*)arg);
	if (buffer == NULL) {
		return NULL;
	}
	char *endptr;
	int64_t value = strtoll(buffer, &endptr, 10);
	bool valid = buffer[0] != 0 && endptr[0] == 0;
	free(buffer);
	if (!valid) {
		// TODO set valueerror
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
		// TODO set valueerror
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
			// TODO set typeerror
			return NULL;
		}
	} else if (args->len == 2) {
		Object *arg0 = args->data[0];
		Object *arg1 = args->data[1];
		if (arg0->type == &g_bytes && arg1->type == &g_int) {
			return (Object*)bytes2int((BytesObject*)arg0, ((IntObject*)arg1)->value);
		} else {
			// TODO set typeerror
		}
	} else {
		// TODO set typeerror
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
			// TODO set typeerror
			return NULL;
		}
	} else {
		// TODO set typeerror
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
	Object *init = get_attr_inner((Object*)result, "__init__");
	if (init) {
		if (!call(init, args)) {
			return NULL;
		}
	}
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
		// TODO set attributeerror
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
		// TODO set attributeerror
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
	if (object_equals(name, (Object*)bytes_raw("len", 3)).equals) {
		return (Object*)int_raw(self->core.len);
	}

	// TODO set attributeerror
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
			// TODO set typeerror
			return NULL;
		}
	} else {
		// TODO set typeerror
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
	if (object_equals(name, (Object*)bytes_raw("len", 3)).equals) {
		return (Object*)int_raw(self->len);
	}

	// TODO set attributeerror
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
			// TODO set typeerror
			return NULL;
		}
	} else {
		// TODO set typeerror
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
	if (object_equals(name, (Object*)bytes_raw("len", 3)).equals) {
		return (Object*)int_raw(self->len);
	}

	// TODO set attributeerror
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
		// this arg (dict) must be a dict
		Object *_arg0 = args->data[0];
		if (_arg0->type != &g_bytes) {
			// TODO set typeerror
			return NULL;
		}
		Object *_arg1 = args->data[1];
		if (_arg1->type != &g_type) {
			// TODO set typeerror
			return NULL;
		}
		Object *_arg2 = args->data[2];
		if (_arg1->type != &g_dict) {
			// TODO set typeerror
			return NULL;
		}
		DictObject *arg2 = (DictObject*)_arg2;
		TypeObject *result = (TypeObject*)gc_malloc(sizeof(TypeObject));
		result->header_basic.header_dict.header = (ObjectHeader) {
			.table = &type_table,
			.type = &g_type,
		};
		result->base_class = (TypeObject*)_arg1,
		result->constructor = object_constructor, // ...I think this is right?
		result->name = _arg0;
		bool inner_tracer(void *key, void **val) {
			dict_set(&result->header_basic.header_dict.core, key, *val, object_hasher, object_equals);
		}
		dict_trace(&arg2->core, inner_tracer);
		return (Object*)result;
	} else {
		// TODO set typeerror
		return NULL;
	}
}

bool type_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	TypeObject *self = (TypeObject*)_self;
	if (!tracer((Object*)self->base_class)) return false;
	if (!tracer(self->name)) return false;
	return true;
}

Object *type_get_attr(Object *self, Object *name) {
	return object_get_attr(self, name);
}

Object *type_call(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject*)_self;
	return self->constructor(_self, args);
}

Object *closure_constructor(Object *self, TupleObject *args) {
	if (args->len != 3) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_bytes) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[1]->type != &g_bytes) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[2]->type != &g_dict) {
		// TODO set typeerror
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
	if (object_equals(name, (Object*)bytes_raw("name", 4)).equals) {
		return (Object*)self->name;
	}
	if (object_equals(name, (Object*)bytes_raw("code", 4)).equals) {
		return (Object*)self->bytecode;
	}

	// TODO set attributeerror
	return NULL;
}

Object *closure_call(Object *self, TupleObject *args) {
	// TODO uhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
}

Object *bytes_constructor(Object *self, TupleObject *args) {
	if (args->len != 1) {
		// TODO set typeerror
		return NULL;
	}
	Object *method = get_attr_inner(self, "__str__");
	if (method == NULL) {
		return NULL;
	}
	Object *result = call(method, tuple_raw(NULL, 0));
	// if we're strapped for bugs we can remove this check
	if (result->type != &g_bytes) {
		// TODO set typeerror
		return NULL;
	}
	return result;
}

char *bytes_data(BytesObject *self) {
	if (self->header.table == &bytes_unowned_table) {
		return ((BytesUnownedObject*)self)->_data;
	} else {
		return self->_data;
	}
}

Object *bytes_get_attr(Object *_self, Object *name) {
	BytesObject *self = (BytesObject*)_self;
	if (object_equals(name, (Object*)bytes_raw("len", 3)).equals) {
		return (Object*)int_raw(self->len);
	}

	// TODO set attributeerror
	return NULL;
}

Object *bytes_slice(Object *_self, size_t start, size_t count) {

}

Object *bool_constructor(Object *self, TupleObject *args) {
	if (args->len != 1) {
		// TODO set typeerror
		return NULL;
	}
	Object *method = get_attr_inner(self, "__bool__");
	if (method == NULL) {
		return NULL;
	}
	Object *result = call(method, tuple_raw(NULL, 0));
	// if we're strapped for bugs we can remove this check
	if (result->type != &g_bool) {
		// TODO set typeerror
		return NULL;
	}
	return result;
}

Object *builtin_constructor(Object *self, TupleObject *args) {
	// TODO set go fuck yourself error
	return NULL;
}

Object *dict_constructor(Object *self, TupleObject *args) {
	// TODO support non-null constructors
	if (args->len != 0) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)dicto_raw();
}

Object *nonetype_constructor(Object *self, TupleObject *args) {
	if (args->len != 0) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)&g_none;
}

Object *boundmeth_constructor(Object *self, TupleObject *args) {
	// TODO set go fuck yourself error
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
	if (object_equals(name, (Object*)bytes_raw("method", 4)).equals) {
		return (Object*)self->method;
	}
	if (object_equals(name, (Object*)bytes_raw("self", 4)).equals) {
		return (Object*)self->self;
	}

	// TODO set attributeerror
	return NULL;
}

Object *boundmeth_call(Object *_self, TupleObject *args) {
	BoundMethodObject *self = (BoundMethodObject*)_self;
	Object **stuff = alloca(sizeof(Object*) * (args->len + 1));
	stuff[0] = self->self;
	memcpy(&stuff[1], args->data, sizeof(Object*) * args->len);
	return call(self->method, tuple_raw(stuff, args->len + 1));
}

bool bytes_unowned_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	BytesUnownedObject *self = (BytesUnownedObject*)_self;
	return tracer(self->owner);
}

Object *bytes_unowned_get_attr(Object *_self, Object *name) {
	BytesUnownedObject *self = (BytesUnownedObject*)_self;
	if (object_equals(name, (Object*)bytes_raw("len", 3)).equals) {
		return (Object*)int_raw(self->header_bytes.len);
	}

	// TODO set attributeerror
	return NULL;
}

Object *slice_constructor(Object *self, TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
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
	if (object_equals(name, (Object*)bytes_raw("start", 5)).equals) {
		return self->start;
	}
	if (object_equals(name, (Object*)bytes_raw("end", 5)).equals) {
		return self->end;
	}

	// TODO set attributeerror
	return NULL;
}

Object *get_attr_inner(Object *self, char *name) {
	Object *temp = (Object*)bytes_raw(name, strlen(name));
	if (!temp) {
		return NULL;
	}
	return get_attr(self, temp);
}
Object *get_attr(Object *self, Object *name) {
	Object *result;
	result = self->table->get_attr(self, name);
	if (result != NULL) {
		return result;
	}
	// is this right?
	if (self->table == &type_table) {
		for (TypeObject *type = ((TypeObject*)self)->base_class; type; type = type->base_class) {
			result = type->header_basic.header_dict.header.table->get_attr((Object*)type, name);
			if (result != NULL) {
				return result;
			}
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
	}

	return NULL;
}
bool set_attr_inner(Object *self, char *name, Object *value) {
	Object *temp = (Object*)bytes_raw(name, strlen(name));
	if (!temp) {
		return NULL;
	}
	return set_attr(self, temp, value);
}
bool set_attr(Object *self, Object *name, Object *value) {
	return self->table->set_attr(self, name, value);
}

bool del_attr_inner(Object *self, char *name) {
	Object *temp = (Object*)bytes_raw(name, strlen(name));
	if (!temp) {
		return NULL;
	}
	return del_attr(self, temp);
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
	Object *result = call(method, tuple_raw(NULL, 0));
	if (result->type != &g_int) {
		// TODO set typeerror
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
	Object *result = call(method, tuple_raw(&val2, 1));
	if (result->type != &g_bool) {
		// TODO set typeerror
		return (EqualityResult) {
			.equals = false,
			.success = false,
		};
	}
	return (EqualityResult) {
		.equals = ((IntObject*)result)->value != 0,
		.success = true,
	};
}

extern MemberInitEntry __start_static_member_adds;
extern MemberInitEntry __stop_static_member_adds;
__attribute__((constructor)) void init_static_adds() {
	for (MemberInitEntry *iter = &__start_static_member_adds; iter != &__stop_static_member_adds; iter++) {
		dict_set(&iter->self->core, (void*)bytes_raw(iter->name, strlen(iter->name)), (void*)iter->value, object_hasher, object_equals);
	}
}
