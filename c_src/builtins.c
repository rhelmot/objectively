#include "object.h"
#include "gc.h"

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
		// TODO return typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_dict) {
		// TODO return typeerror
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];

	GetResult result = dict_get(&self->core, (void*)args->data[1], object_hasher, object_equals);
	if (!result.success) {
		return NULL;
	}
	if (!result.found) {
		// TODO set indexerror
		return NULL;
	}
	return (Object*)result.val;
}
BUILTIN_METHOD(__getitem__, dict_getitem, dict);

Object *dict_setitem(TupleObject *args) {
	if (args->len != 3) {
		// TODO return typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_dict) {
		// TODO return typeerror
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
		// TODO return typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_dict) {
		// TODO return typeerror
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];

	GetResult result = dict_pop(&self->core, (void*)args->data[1], object_hasher, object_equals);
	if (!result.success) {
		return NULL;
	}
	if (!result.found) {
		// TODO set indexerror
		return NULL;
	}
	return (Object*)result.val;
}
BUILTIN_METHOD(pop, dict_popitem, dict);

Object *dict_hash(TupleObject *args) {
	// TODO set typeerror
	return NULL;
}
BUILTIN_METHOD(__hash__, dict_hash, dict);

Object *dict_eq(TupleObject *args) {
	__label__ return_false;

	if (args->len != 2) {
		// TODO return typeerror
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
		Object *key = (Object*)_key;
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

/////////////////////////////////////
/// bytes methods
/////////////////////////////////////

Object *bytes_eq(TupleObject *args) {
	if (args->len != 2) {
		// TODO return typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_bytes || args->data[1]->type != &g_bytes) {
		return false;
	}

	BytesObject *self = (BytesObject*)args->data[0];
	BytesObject *other = (BytesObject*)args->data[1];

	if (self->len != other->len) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(memcmp(bytes_data(self), bytes_data(other), self->len) == 0);
}
BUILTIN_METHOD(__eq__, bytes_eq, bytes);

Object *bytes_getitem(TupleObject *args) {
	if (args->len != 2) {
		// TODO return typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_bytes) {
		// TODO return typeerror
		return NULL;
	}
	BytesObject *self = (BytesObject*)args->data[0];
	// this is not an isinstance check. could spawn bugs?
	if (args->data[1]->type == &g_int) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			// TODO return indexerror
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
			// TODO set typeerror
		}
		if (arg->end->type == &g_int) {
			end = convert_index(self->len, ((IntObject*)arg->end)->value);
		} else if (arg->start->type == &g_nonetype) {
			end = self->len;
		} else {
			// TODO set typeerror
		}
		if (start >= self->len || end > self->len || end < start) {
			// TODO set indexerror
		}
		return (Object*)bytes_unowned_raw(bytes_data(self) + start, end - start, (Object*)self);
	} else {
		// TODO set typeerror
		return NULL;
	}
}
BUILTIN_METHOD(__getitem__, bytes_getitem, bytes);

/////////////////////////////////////
/// list methods
/////////////////////////////////////

BUILTIN_METHOD(__hash__, dict_hash, list);

Object *list_eq(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
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
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_list) {
		// TODO set typeerror
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	if (args->data[1]->type == &g_int) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			// TODO return indexerror
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
			// TODO set typeerror
		}
		if (arg->end->type == &g_int) {
			end = convert_index(self->len, ((IntObject*)arg->end)->value);
		} else if (arg->start->type == &g_nonetype) {
			end = self->len;
		} else {
			// TODO set typeerror
		}
		if (start >= self->len || end > self->len || end < start) {
			// TODO set indexerror
		}
		return (Object*)list_raw(self->data + start, end - start);
	} else {
		// TODO set typeerror
		return NULL;
	}
}
BUILTIN_METHOD(__getitem__, list_getitem, list);

Object *list_setitem(TupleObject *args) {
	if (args->len != 3) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_list) {
		// TODO set typeerror
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	if (args->data[1]->type == &g_int) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			// TODO return indexerror
			return NULL;
		}
		self->data[index] = args->data[2];
		return (Object*)&g_none;
	} else {
		// TODO slices
		// TODO set typeerror
		return NULL;
	}
}
BUILTIN_METHOD(__setitem__, list_setitem, list);

/////////////////////////////////////
/// int methods
/////////////////////////////////////

Object *int_add(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value + ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__add__, int_add, int);

Object *int_sub(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value - ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__sub__, int_sub, int);

Object *int_mul(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value * ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__mul__, int_mul, int);

Object *int_div(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	int64_t divisor = ((IntObject*)args->data[1])->value;
	if (divisor == 0) {
		// TODO set zerodivisionerror
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value / divisor);
}
BUILTIN_METHOD(__div__, int_div, int);

Object *int_mod(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	int64_t divisor = ((IntObject*)args->data[1])->value;
	if (divisor == 0) {
		// TODO set zerodivisionerror
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value % divisor);
}
BUILTIN_METHOD(__mod__, int_mod, int);

Object *int_and(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value & ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__and__, int_and, int);

Object *int_or(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value | ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__or__, int_or, int);

Object *int_xor(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)int_raw(((IntObject*)args->data[0])->value ^ ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__xor__, int_xor, int);

Object *int_not(TupleObject *args) {
	if (args->len != 1) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)int_raw(~((IntObject*)args->data[0])->value);
}
BUILTIN_METHOD(__invert__, int_not, int);

Object *int_eq(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
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
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int) {
		// TODO set typeerror
		return NULL;
	}
	// return self?
	return (Object*)int_raw(((IntObject*)args->data[0])->value);
}

Object *int_gt(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
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
		// TODO set typeerror
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
		// TODO set typeerror
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
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_int || args->data[1]->type != &g_int) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((IntObject*)args->data[0])->value <= ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__le__, int_le, int);

/////////////////////////////////////
/// bool methods
/////////////////////////////////////

Object *bool_not(TupleObject *args) {
	if (args->len != 1) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_bool) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)int_raw(!((IntObject*)args->data[0])->value);
}
BUILTIN_METHOD(__not__, bool_not, int);

Object *bool_hash(TupleObject *args) {
	if (args->len != 1) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_bool) {
		// TODO set typeerror
		return NULL;
	}
	return (EmptyObject*)args->data[0] == &g_true ? (Object*)int_raw(1) : (Object*)int_raw(0);
}

/////////////////////////////////////
/// float methods
/////////////////////////////////////

Object *float_add(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)float_raw(((FloatObject*)args->data[0])->value + ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__add__, float_add, float);

Object *float_sub(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)float_raw(((FloatObject*)args->data[0])->value - ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__sub__, float_sub, float);

Object *float_mul(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)float_raw(((FloatObject*)args->data[0])->value * ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__mul__, float_mul, float);

Object *float_div(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		// TODO set typeerror
		return NULL;
	}
	double divisor = ((FloatObject*)args->data[1])->value;
	if (divisor == 0) {
		// TODO set zerodivisionerror
		return NULL;
	}
	return (Object*)float_raw(((FloatObject*)args->data[0])->value / divisor);
}
BUILTIN_METHOD(__div__, float_div, float);

Object *float_eq(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)bool_raw(((FloatObject*)args->data[0])->value == ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__eq__, float_eq, float);

Object *float_hash(TupleObject *args) {
	if (args->len != 1) {
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_float) {
		// TODO set typeerror
		return NULL;
	}
	// return int(self) for integral floats?
	return (Object*)float_raw(*(int64_t*)&((FloatObject*)args->data[0])->value);
}

Object *float_gt(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
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
		// TODO set typeerror
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
		// TODO set typeerror
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
		// TODO set typeerror
		return NULL;
	}
	if (args->data[0]->type != &g_float || args->data[1]->type != &g_float) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((FloatObject*)args->data[0])->value <= ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__le__, float_le, float);

/////////////////////////////////////
/// object methods
/////////////////////////////////////

Object *object_eq(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	return (Object*)bool_raw(args->data[0] == args->data[1]);
}
BUILTIN_METHOD(__eq__, object_eq, object);

Object *object_ne(TupleObject *args) {
	if (args->len != 2) {
		// TODO set typeerror
		return NULL;
	}
	Object *eq = get_attr_inner(args->data[0], "__eq__");
	if (eq == NULL) {
		return NULL;
	}
	Object *is_eq = call(eq, tuple_raw(&args->data[1], 1));
	if (is_eq == NULL) {
		return NULL;
	}
	Object *not = get_attr_inner(args->data[0], "__not__");
	if (not == NULL) {
		return NULL;
	}
	return call(not, tuple_raw(NULL, 0));
}
BUILTIN_METHOD(__ne__, object_ne, object);

Object *object_hash(TupleObject *args) {
	if (args->len != 1) {
		// TODO set typeerror
		return NULL;
	}

	return (Object*)int_raw((int64_t)args->data[0]);
}
BUILTIN_METHOD(__hash__, object_hash, object);
