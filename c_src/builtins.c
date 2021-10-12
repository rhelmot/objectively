#include <string.h>
#include <stdio.h>
#include <alloca.h>

#include "object.h"
#include "gc.h"
#include "errors.h"

#define BUILTIN_METHOD(name, function, cls) \
BuiltinFunctionObject g_##cls##_##name = { \
	.header = { \
		.type = &g_builtin, \
		.table = &builtinfunction_table, \
	}, \
	.func = function, \
}; \
STATIC_OBJECT(g_##cls##_##name); \
ADD_MEMBER(g_##cls, #name, g_##cls##_##name)

#define BUILTIN_FUNCTION(name, function) \
BuiltinFunctionObject g_##name = { \
	.header = { \
		.type = &g_builtin, \
		.table = &builtinfunction_table, \
	}, \
	.func = function, \
}; \
STATIC_OBJECT(g_##name); \
ADD_MEMBER(builtins, #name, g_##name)

DictObject builtins = {
	.header = {
		.table = &dicto_table,
		.type = &g_dict,
	},
};
STATIC_OBJECT(builtins);

size_t convert_index(size_t len, int64_t idx) {
	if (idx < 0) {
		idx = len + idx;
	}
	return (size_t)idx;
}

/////////////////////////////////////
/// dict methods
/////////////////////////////////////

Object *dict_getitem(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_dict) {
		error = exc_msg(&g_TypeError, "Expected dict");
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];

	GetResult result = dict_get(&self->core, (void*)args->data[1], object_hasher, object_equals);
	if (!result.success) {
		return NULL;
	}
	if (!result.found) {
		error = exc_arg(&g_KeyError, args->data[1]);
		return NULL;
	}
	return (Object*)result.val;
}
BUILTIN_METHOD(__getitem__, dict_getitem, dict);

Object *dict_setitem(TupleObject *args) {
	if (args->len != 3) {
		error = exc_msg(&g_TypeError, "Expected 3 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_dict) {
		error = exc_msg(&g_TypeError, "Expected dict");
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];

	if (!dict_set(&self->core, (void*)args->data[1], (void*)args->data[2], object_hasher, object_equals)) {
		return NULL;
	}
	return (Object*)&g_none;
}
BUILTIN_METHOD(__setitem__, dict_setitem, dict);

Object *dict_popitem(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_dict) {
		error = exc_msg(&g_TypeError, "Expected dict");
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];

	GetResult result = dict_pop(&self->core, (void*)args->data[1], object_hasher, object_equals);
	if (!result.success) {
		return NULL;
	}
	if (!result.found) {
		error = exc_arg(&g_KeyError, args->data[1]);
		return NULL;
	}
	return (Object*)result.val;
}
BUILTIN_METHOD(pop, dict_popitem, dict);

Object *dict_delitem(TupleObject *args) {
	return dict_popitem(args) == NULL ? NULL : (Object*)&g_none;
}
BUILTIN_METHOD(__delitem__, dict_delitem, dict);

Object *dict_hash(TupleObject *args) {
	error = exc_msg(&g_TypeError, "Unhashable");
	return NULL;
}
BUILTIN_METHOD(__hash__, dict_hash, dict);

Object *dict_eq(TupleObject *args) {
	__label__ return_false;

	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_dict || args->data[1]->type != &g_dict) {
		return (Object*)bool_raw(false);
	}
	DictObject *self = (DictObject*)args->data[0];
	DictObject *other = (DictObject*)args->data[1];

	if (self->core.len != other->core.len) {
		return (Object*)bool_raw(false);
	}

	bool inner_tracer(void *_key, void **_val) {
		Object *val = (Object*)*_val;
		GetResult get = dict_get(&other->core, _key, object_hasher, object_equals);
		if (!get.success) {
			return false;
		}
		if (!get.found) {
			// thanks gcc!
			goto return_false;
		}

		EqualityResult eq = object_equals(val, get.val);
		if (!eq.success) {
			return false;
		}
		if (!eq.equals) {
			goto return_false;
		}
		return true;
	}

	if (!dict_trace(&self->core, inner_tracer)) {
		return NULL;
	};
	return (Object*)bool_raw(true);

return_false:
	return (Object*)bool_raw(false);

}
BUILTIN_METHOD(__eq__, dict_eq, dict);

Object *dict_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
	}
	if (args->data[0]->type != &g_dict) {
		error = exc_msg(&g_TypeError, "Expected dict");
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];
	return (Object*)bool_raw(self->core.len != 0);
}
BUILTIN_METHOD(__bool__, dict_bool, dict);

/////////////////////////////////////
/// bytes methods
/////////////////////////////////////

Object *bytes_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_bytes || args->data[1]->type != &g_bytes) {
		return (Object*)bool_raw(false);
	}

	BytesObject *self = (BytesObject*)args->data[0];
	BytesObject *other = (BytesObject*)args->data[1];

	if (self->len != other->len) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(memcmp(bytes_data(self), bytes_data(other), self->len) == 0);
}
BUILTIN_METHOD(__eq__, bytes_eq, bytes);

Object *bytes_hash(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_bytes) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}

	BytesObject *self = (BytesObject*)args->data[0];
	uint64_t result = 0xfedcba9876543210;
	unsigned char *data = (unsigned char *)bytes_data(self);
	for (size_t i = 0; i < self->len; i++) {
		result += data[i];
		result = (result << 23) | (result >> (64-23));
	}
	return (Object*)int_raw((int64_t)result);
}
BUILTIN_METHOD(__hash__, bytes_hash, bytes);

Object *bytes_getitem(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_bytes) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}
	BytesObject *self = (BytesObject*)args->data[0];
	if (args->data[1]->type == &g_int) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return (Object*)int_raw(bytes_data(self)[index]);
	} else if (args->data[1]->type == &g_slice) {
		SliceObject *arg = (SliceObject*)args->data[1];
		size_t start, end;
		if (arg->start->type == &g_int) {
			start = convert_index(self->len, ((IntObject*)arg->start)->value);
		} else if (arg->start->type == &g_nonetype) {
			start = 0;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (arg->end->type == &g_int) {
			end = convert_index(self->len, ((IntObject*)arg->end)->value);
		} else if (arg->start->type == &g_nonetype) {
			end = self->len;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (start >= self->len || end > self->len || end < start) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return (Object*)bytes_unowned_raw(bytes_data(self) + start, end - start, (Object*)self);
	} else {
		error = exc_msg(&g_TypeError, "Expected int or slice");
		return NULL;
	}
}
BUILTIN_METHOD(__getitem__, bytes_getitem, bytes);

Object *bytes_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_bytes) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}
	BytesObject *self = (BytesObject*)args->data[0];
	return (Object*)bool_raw(self->len != 0);
}
BUILTIN_METHOD(__bool__, bytes_bool, bytes);

Object *bytes_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_bytes) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}
	return args->data[0];
}
BUILTIN_METHOD(__str__, bytes_str, bytes);

/////////////////////////////////////
/// tuple methods
/////////////////////////////////////

Object *tuple_hash(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_tuple) {
		error = exc_msg(&g_TypeError, "Expected tuple");
		return NULL;
	}
	TupleObject *self = (TupleObject*)args->data[0];
	uint64_t hash = 0x0123456789abcdef;
	for (size_t i = 0; i < self->len; i++) {
		HashResult hashed = object_hasher(self->data[i]);
		if (!hashed.success) {
			return NULL;
		}
		hash += hashed.hash;
		hash = (hash << 17) | (hash >> (64-17));
	}
	return (Object*)int_raw((int64_t)hash);
}
BUILTIN_METHOD(__hash__, tuple_hash, tuple);

Object *tuple_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_tuple || args->data[1]->type != &g_tuple) {
		return (Object*)bool_raw(false);
	}
	TupleObject *self = (TupleObject*)args->data[0];
	TupleObject *other = (TupleObject*)args->data[1];

	for (size_t i = 0; i < self->len && i < other->len; i++) {
		EqualityResult eq = object_equals(self->data[i], other->data[i]);
		if (!eq.success) {
			return NULL;
		}
		if (!eq.equals) {
			return (Object*)bool_raw(false);
		}
	}
	return (Object*)bool_raw(true);
}
BUILTIN_METHOD(__eq__, tuple_eq, tuple);

Object *tuple_getitem(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_tuple) {
		error = exc_msg(&g_TypeError, "Expected tuple");
		return NULL;
	}
	TupleObject *self = (TupleObject*)args->data[0];
	if (args->data[1]->type == &g_int) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return self->data[index];
	} else if (args->data[1]->type == &g_slice) {
		SliceObject *arg = (SliceObject*)args->data[1];
		size_t start, end;
		if (arg->start->type == &g_int) {
			start = convert_index(self->len, ((IntObject*)arg->start)->value);
		} else if (arg->start->type == &g_nonetype) {
			start = 0;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (arg->end->type == &g_int) {
			end = convert_index(self->len, ((IntObject*)arg->end)->value);
		} else if (arg->start->type == &g_nonetype) {
			end = self->len;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (start >= self->len || end > self->len || end < start) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return (Object*)tuple_raw(self->data + start, end - start);
	} else {
		error = exc_msg(&g_TypeError, "Expected int or slice");
		return NULL;
	}
}
BUILTIN_METHOD(__getitem__, tuple_getitem, tuple);

Object *tuple_add(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_tuple || args->data[1]->type != &g_tuple) {
		return (Object*)bool_raw(false);
	}
	TupleObject *self = (TupleObject*)args->data[0];
	TupleObject *other = (TupleObject*)args->data[1];

	// overflow here should be physically impossible
	Object **new_data = alloca(sizeof(Object*) * (self->len + other->len));
	memcpy(new_data, self->data, sizeof(Object*) * self->len);
	memcpy(new_data + self->len, other->data, sizeof(Object*) * other->len);
	return (Object*)tuple_raw(new_data, self->len + other->len);
}
BUILTIN_METHOD(__add__, tuple_add, tuple);

Object *tuple_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_tuple) {
		error = exc_msg(&g_TypeError, "Expected tuple");
		return NULL;
	}
	TupleObject *self = (TupleObject*)args->data[0];
	return (Object*)bool_raw(self->len != 0);
}
BUILTIN_METHOD(__bool__, tuple_bool, tuple);

/////////////////////////////////////
/// list methods
/////////////////////////////////////

BUILTIN_METHOD(__hash__, dict_hash, list);

Object *list_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_list || args->data[1]->type != &g_list) {
		return (Object*)bool_raw(false);
	}
	ListObject *self = (ListObject*)args->data[0];
	ListObject *other = (ListObject*)args->data[1];

	for (size_t i = 0; i < self->len && i < other->len; i++) {
		EqualityResult eq = object_equals(self->data[i], other->data[i]);
		if (!eq.success) {
			return NULL;
		}
		if (!eq.equals) {
			return (Object*)bool_raw(false);
		}
	}
	return (Object*)bool_raw(true);
}
BUILTIN_METHOD(__eq__, list_eq, list);

Object *list_getitem(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_list) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	if (args->data[1]->type == &g_int) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return self->data[index];
	} else if (args->data[1]->type == &g_slice) {
		SliceObject *arg = (SliceObject*)args->data[1];
		size_t start, end;
		if (arg->start->type == &g_int) {
			start = convert_index(self->len, ((IntObject*)arg->start)->value);
		} else if (arg->start->type == &g_nonetype) {
			start = 0;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (arg->end->type == &g_int) {
			end = convert_index(self->len, ((IntObject*)arg->end)->value);
		} else if (arg->start->type == &g_nonetype) {
			end = self->len;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (start >= self->len || end > self->len || end < start) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return (Object*)list_raw(self->data + start, end - start);
	} else {
		error = exc_msg(&g_TypeError, "Expected int or slice");
		return NULL;
	}
}
BUILTIN_METHOD(__getitem__, list_getitem, list);

Object *list_setitem(TupleObject *args) {
	if (args->len != 3) {
		error = exc_msg(&g_TypeError, "Expected 3 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_list) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	if (args->data[1]->type == &g_int) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		self->data[index] = args->data[2];
		return (Object*)&g_none;
	} else {
		// TODO slices
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
}
BUILTIN_METHOD(__setitem__, list_setitem, list);

Object *list_push(TupleObject *args) {
	if (args->len != 2 && args->len != 3) {
		error = exc_msg(&g_TypeError, "Expected 2 or 3 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_list) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	size_t index;
	if (args->len == 3 && args->data[2]->type == &g_int) {
		index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index > self->len) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
	} else if ((args->len == 3 && args->data[2] == (Object*)&g_none) || args->len == 2) {
		index = self->len;
	} else {
		error = exc_msg(&g_TypeError, "expected nonetype or int"); // is this right?
		return NULL;
	}

	if (self->len >= self->cap) {
		Object **new_data = realloc(self->data, sizeof(Object*) * (self->cap + 1) * 2);
		if (new_data == NULL) {
			error = (Object*)&MemoryError_inst;
			return NULL;
		}
		self->data = new_data;
		self->cap = (self->cap + 1) * 2;
	}

	memmove(&self->data[index], &self->data[index + 1], sizeof(Object*) * (self->len - index));
	self->data[index] = args->data[1];
	self->len++;
	return (Object*)&g_none;
}
BUILTIN_METHOD(push, list_push, list);

bool list_push_back_inner(ListObject *self, Object *item) {
	Object *args[] = {(Object*)self, item};
	TupleObject *inner_args = tuple_raw(args, 2);
	if (inner_args == NULL) {
		return NULL;
	}
	gc_root((Object*)inner_args);
	bool result = list_push(inner_args) != NULL;
	gc_unroot((Object*)inner_args);
	return result;
}

Object *list_pop(TupleObject *args) {
	if (args->len != 1 && args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 1 or 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_list) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	size_t index;
	if (args->len == 2 && args->data[1]->type == &g_int) {
		index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
	} else if ((args->len == 2 && args->data[2] == (Object*)&g_none) || args->len == 1) {
		if (self->len == 0) {
			Object *the_int = (Object*)int_raw(-1);
			if (the_int == NULL) {
				return NULL;
			}
			error = exc_arg(&g_IndexError, the_int);
			return NULL;
		}
		index = self->len - 1;
	} else {
		error = exc_msg(&g_TypeError, "Expected int or nonetype");
		return NULL;
	}

	Object *result = self->data[index];
	memmove(&self->data[index + 1], &self->data[index], sizeof(Object*) * (self->len - index - 1));
	self->len--;
	return result;
}
BUILTIN_METHOD(pop, list_pop, list);

Object *list_pop_back_inner(ListObject *self) {
	TupleObject *inner_args = tuple_raw((Object**)&self, 1);
	if (inner_args == NULL) {
		return NULL;
	}
	gc_root((Object*)inner_args);
	Object *result = list_pop(inner_args);
	gc_unroot((Object*)inner_args);
	return result;
}

// TODO list_delitem

Object *list_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
	}
	if (args->data[0]->type != &g_list) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	return (Object*)bool_raw(self->len != 0);
}
BUILTIN_METHOD(__bool__, list_bool, list);

/////////////////////////////////////
/// int methods
/////////////////////////////////////

Object *int_add(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value + ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__add__, int_add, int);

Object *int_sub(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value - ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__sub__, int_sub, int);

Object *int_mul(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value * ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__mul__, int_mul, int);

Object *int_div(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	int64_t divisor = ((IntObject*)args->data[1])->value;
	if (divisor == 0) {
		error = exc_msg(&g_ZeroDivisionError, "Division by zero");
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value / divisor);
}
BUILTIN_METHOD(__div__, int_div, int);

Object *int_mod(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	int64_t divisor = ((IntObject*)args->data[1])->value;
	if (divisor == 0) {
		error = exc_msg(&g_ZeroDivisionError, "Division by zero");
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value % divisor);
}
BUILTIN_METHOD(__mod__, int_mod, int);

Object *int_and(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value & ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__and__, int_and, int);

Object *int_or(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value | ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__or__, int_or, int);

Object *int_xor(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value ^ ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__xor__, int_xor, int);

Object *int_not(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw(~((IntObject*)args->data[0])->value);
}
BUILTIN_METHOD(__invert__, int_not, int);

Object *int_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((IntObject*)args->data[0])->value == ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__eq__, int_eq, int);

Object *int_hash(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	// return self?
	return (Object*)int_raw(((IntObject*)args->data[0])->value);
}

Object *int_gt(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((IntObject*)args->data[0])->value > ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__gt__, int_gt, int);

Object *int_lt(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((IntObject*)args->data[0])->value < ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__lt__, int_lt, int);

Object *int_ge(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((IntObject*)args->data[0])->value >= ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__ge__, int_ge, int);

Object *int_le(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((IntObject*)args->data[0])->value <= ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__le__, int_le, int);

Object *int_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
	}
	if (args->data[0]->type != &g_int) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	IntObject *self = (IntObject*)args->data[0];
	return (Object*)bool_raw(self->value != 0);
}
BUILTIN_METHOD(__bool__, int_bool, int);

/////////////////////////////////////
/// bool methods
/////////////////////////////////////

Object *bool_not(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_bool) {
		error = exc_msg(&g_TypeError, "Expected bool");
		return NULL;
	}
	return (Object*)bool_raw(!((IntObject*)args->data[0])->value);
}
BUILTIN_METHOD(__not__, bool_not, int);

Object *bool_hash(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_bool) {
		error = exc_msg(&g_TypeError, "Expected bool");
		return NULL;
	}
	return (EmptyObject*)args->data[0] == &g_true ? (Object*)int_raw(1) : (Object*)int_raw(0);
}

Object *bool_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_bool) {
		error = exc_msg(&g_TypeError, "Expected bool");
		return NULL;
	}
	return args->data[0];
}
BUILTIN_METHOD(__bool__, bool_bool, bool);

/////////////////////////////////////
/// float methods
/////////////////////////////////////

Object *float_add(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	return (Object*)float_raw(((FloatObject*)args->data[0])->value + ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__add__, float_add, float);

Object *float_sub(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	return (Object*)float_raw(((FloatObject*)args->data[0])->value - ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__sub__, float_sub, float);

Object *float_mul(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	return (Object*)float_raw(((FloatObject*)args->data[0])->value * ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__mul__, float_mul, float);

Object *float_div(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	double divisor = ((FloatObject*)args->data[1])->value;
	if (divisor == 0) {
		error = exc_msg(&g_ZeroDivisionError, "Division by zero");
		return NULL;
	}
	return (Object*)float_raw(((FloatObject*)args->data[0])->value / divisor);
}
BUILTIN_METHOD(__div__, float_div, float);

Object *float_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	return (Object*)bool_raw(((FloatObject*)args->data[0])->value == ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__eq__, float_eq, float);

Object *float_hash(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (args->data[0]->type != &g_float) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	// return int(self) for integral floats?
	return (Object*)float_raw(*(int64_t*)&((FloatObject*)args->data[0])->value);
}

Object *float_gt(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((FloatObject*)args->data[0])->value > ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__gt__, float_gt, float);

Object *float_lt(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((FloatObject*)args->data[0])->value < ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__lt__, float_lt, float);

Object *float_ge(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((FloatObject*)args->data[0])->value >= ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__ge__, float_ge, float);

Object *float_le(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((FloatObject*)args->data[0])->value <= ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__le__, float_le, float);

Object *float_bool(TupleObject *args) {
	if (args->len != 1 || args->data[0]->type != &g_float) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	FloatObject *self = (FloatObject*)args->data[0];
	return (Object*)bool_raw(self->value != 0.);
}
BUILTIN_METHOD(__bool__, float_bool, float);

/////////////////////////////////////
/// object methods
/////////////////////////////////////

Object *object_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	return (Object*)bool_raw(args->data[0] == args->data[1]);
}
BUILTIN_METHOD(__eq__, object_eq, object);

Object *object_ne(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	Object *eq = get_attr_inner(args->data[0], "__eq__");
	if (eq == NULL) {
		return NULL;
	}
	TupleObject *new_args = tuple_raw(&args->data[1], 1);
	if (new_args == NULL) {
		return NULL;
	}
	gc_root((Object*)new_args);
	Object *is_eq = call(eq, new_args);
	gc_unroot((Object*)new_args);
	if (is_eq == NULL) {
		return NULL;
	}
	Object *not = get_attr_inner(args->data[0], "__not__");
	if (not == NULL) {
		return NULL;
	}
	new_args = tuple_raw(NULL, 0);
	if (new_args == NULL) {
		return NULL;
	}
	gc_root((Object*)new_args);
	Object *result = call(not, new_args);
	gc_unroot((Object*)new_args);
	return result;
}
BUILTIN_METHOD(__ne__, object_ne, object);

Object *object_hash(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}

	return (Object*)int_raw((int64_t)args->data[0]);
}
BUILTIN_METHOD(__hash__, object_hash, object);

Object *object_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	return (Object*)bool_raw(true);
}
BUILTIN_METHOD(__bool__, object_bool, object);

/////////////////////////////////////
/// freestanding methods
/////////////////////////////////////

Object *builtin_print(TupleObject *args) {
	for (size_t i = 0; i < args->len; i++) {
		if (i != 0) {
			putchar(' ');
		}
		// TODO null bytes
		BytesObject *as_str = bytes_constructor_inner(args->data[i]);
		if (as_str == NULL) {
			return NULL;
		}
		fwrite(bytes_data(as_str), 1, as_str->len, stdout);
	}
	puts("");
	return (Object*)&g_none;
}
BUILTIN_FUNCTION(print, builtin_print);
