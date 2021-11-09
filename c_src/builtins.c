#include <string.h>
#include <stdio.h>
#include <alloca.h>
#include <stdarg.h>
#include <time.h>

#include "builtins.h"
#include "object.h"
#include "gc.h"
#include "errors.h"
#include "thread.h"

DictObject builtins = {
	.header = {
		.table = &dicto_table,
		.type = &g_dict,
		.group = &root_threadgroup,
	},
};
STATIC_OBJECT(builtins);

size_t convert_index(size_t len, int64_t idx) {
	if (idx < 0) {
		idx = len + idx;
	}
	return (size_t)idx;
}
Object *own_iter(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	return args->data[0];
}

/////////////////////////////////////
// list iterator declaration and methods
/////////////////////////////////////

typedef struct ListIterator {
	ObjectHeader header;
	Object *child;
	size_t next_index;
} ListIterator;

bool list_iterator_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	ListIterator *self = (ListIterator*)_self;
	if (!tracer(self->child)) return false;
	return true;
}

ObjectTable list_iterator_table = {
	.trace = list_iterator_trace,
	.finalize = null_finalize,
	.get_attr = null_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
	.size.given = sizeof(ListIterator),
};

BUILTIN_TYPE(list_iterator, object, null_constructor);

Object *list_iter_next(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_list_iterator)) {
		error = exc_msg(&g_TypeError, "Expected list iterator");
		return NULL;
	}

	ListIterator *self = (ListIterator*)args->data[0];
	Object *method = get_attr_inner(self->child, "__getitem__");
	if (method == NULL) {
		return NULL;
	}
	IntObject *index = int_raw(self->next_index);
	if (index == NULL) {
		return NULL;
	}
	TupleObject *inner_args = tuple_raw((Object**)&index, 1);
	if (args == NULL) {
		return NULL;
	}
	gc_root((Object*)inner_args);
	Object *result = call(method, inner_args);
	gc_unroot((Object*)inner_args);
	if (result == NULL) {
		if (isinstance_inner(error, &g_IndexError)) {
			error = exc_nil(&g_StopIteration);
		}
		return NULL;
	}
	self->next_index++;
	return result;
}
BUILTIN_METHOD(__next__, list_iter_next, list_iterator);
BUILTIN_METHOD(__iter__, own_iter, list_iterator);

/////////////////////////////////////
/// dict methods
/////////////////////////////////////

Object *dict_getitem(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_dict)) {
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
	if (!isinstance_inner(args->data[0], &g_dict)) {
		error = exc_msg(&g_TypeError, "Expected dict");
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];
	if (CURRENT_GROUP != self->header.group && !dict_get(&self->core, (void*)args->data[1], object_hasher, object_equals).found) {
		error = exc_msg(&g_RuntimeError, "Cannot allocate space in another group");
		return NULL;
	}
	void *other_alloc(size_t size) { return quota_alloc(size, self->header.group); }
	void other_dealloc(void * ptr, size_t size) { quota_dealloc(ptr, size, self->header.group); }

	if (!dict_set(&self->core, (void*)args->data[1], (void*)args->data[2], object_hasher, object_equals, other_alloc, other_dealloc)) {
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
	if (!isinstance_inner(args->data[0], &g_dict)) {
		error = exc_msg(&g_TypeError, "Expected dict");
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];
	void other_dealloc(void * ptr, size_t size) { quota_dealloc(ptr, size, self->header.group); }

	GetResult result = dict_pop(&self->core, (void*)args->data[1], object_hasher, object_equals, other_dealloc);
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

Object *dict_hash(TupleObject *_args) {
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
	if (!isinstance_inner(args->data[0], &g_dict )|| !isinstance_inner(args->data[1], &g_dict)) {
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
	if (!isinstance_inner(args->data[0], &g_dict)) {
		error = exc_msg(&g_TypeError, "Expected dict");
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];
	return (Object*)bool_raw(self->core.len != 0);
}
BUILTIN_METHOD(__bool__, dict_bool, dict);

Object *dict_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_dict)) {
		error = exc_msg(&g_TypeError, "Expected dict");
		return NULL;
	}
	DictObject *self = (DictObject*)args->data[0];
	TupleObject *converted = tuple_raw(NULL, self->core.len);
	if (converted == NULL) {
		return NULL;
	}
	gc_root((Object*)converted);
	size_t i = 0;
	bool tracer(void *_key, void **_val) {
		Object *key = (Object*)_key;
		Object *val = (Object*)*_val;
		converted->data[i] = format_inner("%s: %s", key, val);
		if (converted->data[i] == NULL) {
			return false;
		}
		i++;
		return true;
	}
	if (!dict_trace(&self->core, tracer)) {
		gc_unroot((Object*)converted);
		return NULL;
	}
	TupleObject *inner_args = tuple_raw(NULL, 2);
	if (inner_args == NULL) {
		gc_unroot((Object*)converted);
		return NULL;
	}
	inner_args->data[0] = (Object*)bytes_unowned_raw(", ", 2, NULL);
	if (inner_args->data[0] == NULL) {
		gc_unroot((Object*)converted);
		return NULL;
	}
	inner_args->data[1] = (Object*)converted;
	gc_unroot((Object*)converted);
	gc_root((Object*)inner_args);
	Object *joined_str = bytes_join(inner_args);
	gc_unroot((Object*)inner_args);
	if (joined_str == NULL) {
		return NULL;
	}
	gc_root((Object*)joined_str);
	Object *result = format_inner("{%s}", joined_str);
	gc_unroot((Object*)joined_str);
	return result;
}
BUILTIN_METHOD(__str__, dict_str, dict);

/////////////////////////////////////
/// bytes methods
/////////////////////////////////////

Object *bytes_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if ((!isinstance_inner(args->data[0], &g_bytes) && !isinstance_inner(args->data[0], &g_bytearray)) || 
	    (!isinstance_inner(args->data[1], &g_bytes) && !isinstance_inner(args->data[1], &g_bytearray))) {
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
BUILTIN_METHOD(__eq__, bytes_eq, bytearray);

Object *bytes_hash(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (isinstance_inner(args->data[0], &g_bytearray)) {
		error = exc_msg(&g_TypeError, "Unhashable");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bytes)) {
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
BUILTIN_METHOD(__hash__, bytes_hash, bytearray);

Object *bytes_getitem(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bytes) && !isinstance_inner(args->data[0], &g_bytearray)) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}
	BytesObject *self = (BytesObject*)args->data[0];
	if (isinstance_inner(args->data[1], &g_int)) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return (Object*)int_raw((unsigned char)bytes_data(self)[index]);
	} else if (isinstance_inner(args->data[1], &g_slice)) {
		SliceObject *arg = (SliceObject*)args->data[1];
		size_t start, end;
		if (isinstance_inner(arg->start, &g_int)) {
			start = convert_index(self->len, ((IntObject*)arg->start)->value);
		} else if (isinstance_inner(arg->start, &g_nonetype)) {
			start = 0;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (isinstance_inner(arg->end, &g_int)) {
			end = convert_index(self->len, ((IntObject*)arg->end)->value);
		} else if (isinstance_inner(arg->end, &g_nonetype)) {
			end = self->len;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (start > self->len || end > self->len || end < start) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		if (isinstance_inner((Object*)self, &g_bytearray)) {
			return (Object*)bytearray_raw(bytes_data(self) + start, end - start, self->header.type);
		} else {
			return (Object*)bytes_unowned_raw_ex(bytes_data(self) + start, end - start, (Object*)self, self->header.type);
		}
	} else {
		error = exc_msg(&g_TypeError, "Expected int or slice");
		return NULL;
	}
}
BUILTIN_METHOD(__getitem__, bytes_getitem, bytes);
BUILTIN_METHOD(__getitem__, bytes_getitem, bytearray);

Object *bytes_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bytes) && !isinstance_inner(args->data[0], &g_bytearray)) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}
	BytesObject *self = (BytesObject*)args->data[0];
	return (Object*)bool_raw(self->len != 0);
}
BUILTIN_METHOD(__bool__, bytes_bool, bytes);
BUILTIN_METHOD(__bool__, bytes_bool, bytearray);

Object *bytes_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bytes)) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}
	return args->data[0];
}
BUILTIN_METHOD(__str__, bytes_str, bytes);

Object *bytes_add(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if ((!isinstance_inner(args->data[0], &g_bytes) && !isinstance_inner(args->data[0], &g_bytearray)) ||
	    (!isinstance_inner(args->data[1], &g_bytes) && !isinstance_inner(args->data[1], &g_bytearray))) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}
	BytesObject *self = (BytesObject*)args->data[0];
	BytesObject *other = (BytesObject*)args->data[1];
	BytesObject *result;
	if (isinstance_inner(args->data[0], &g_bytearray)) {
		result = (BytesObject*)bytearray_raw(NULL, self->len + other->len, self->header.type);
	} else {
		result = bytes_raw_ex(NULL, self->len + other->len, self->header.type);
	}
	if (result == NULL) {
		return NULL;
	}
	memcpy((char*)bytes_data(result), bytes_data(self), self->len);
	memcpy((char*)bytes_data(result) + self->len, bytes_data(other), other->len);
	return (Object*)result;
}
BUILTIN_METHOD(__add__, bytes_add, bytes);
BUILTIN_METHOD(__add__, bytes_add, bytearray);

Object *bytes_mul(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bytes) && !isinstance_inner(args->data[0], &g_bytearray)) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}
	if (!isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	BytesObject *self = (BytesObject*)args->data[0];
	size_t times = (size_t)((IntObject*)args->data[1])->value;
	size_t new_len = times * self->len;
	if (times != 0 && new_len / times != self->len) {
		error = exc_msg(&g_ValueError, "Integer overflow");
		return NULL;
	}
	BytesObject *result;
	if (isinstance_inner(args->data[0], &g_bytearray)) {
		result = (BytesObject*)bytearray_raw(NULL, new_len, self->header.type);
	} else {
		result = bytes_raw_ex(NULL, new_len, self->header.type);
	}
	if (result == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < times; i++) {
		memcpy((char*)bytes_data(result) + i * self->len, bytes_data(self), self->len);
	}
	return (Object*)result;
}
BUILTIN_METHOD(__mul__, bytes_mul, bytes);
BUILTIN_METHOD(__mul__, bytes_mul, bytearray);

Object *bytes_join(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bytes)) {
		error = exc_msg(&g_TypeError, "Expected bytes");
		return NULL;
	}
	BytesObject *self = (BytesObject*)args->data[0];
	Object **to_join;
	size_t join_count;
	if (isinstance_inner(args->data[1], &g_tuple)) {
		to_join = ((TupleObject*)args->data[1])->data;
		join_count = ((TupleObject*)args->data[1])->len;
	} else if (isinstance_inner(args->data[1], &g_list)) {
		to_join = ((ListObject*)args->data[1])->data;
		join_count = ((ListObject*)args->data[1])->len;
	} else {
		error = exc_msg(&g_TypeError, "Expected list or tuple");
		return NULL;
	}

	size_t total_size = 0;
	for (size_t i = 0; i < join_count; i++) {
		if (i != 0) {
			total_size += self->len;
			if (total_size < self->len) {
				error = exc_msg(&g_ValueError, "Integer overflow");
				return NULL;
			}
		}
		if (!isinstance_inner(to_join[i], &g_bytes)) {
			error = exc_msg(&g_TypeError, "Expected bytes");
			return NULL;
		}
		BytesObject *arg = (BytesObject*)to_join[i];
		total_size += arg->len;
		if (total_size < arg->len) {
			error = exc_msg(&g_ValueError, "Integer overflow");
			return NULL;
		}
	}

	BytesObject *result = bytes_raw_ex(NULL, total_size, self->header.type);
	if (result == NULL) {
		return NULL;
	}
	total_size = 0;
	for (size_t i = 0; i < join_count; i++) {
		if (i != 0) {
			memcpy((char*)bytes_data(result) + total_size, bytes_data(self), self->len);
			total_size += self->len;
		}
		BytesObject *arg = (BytesObject*)to_join[i];
		memcpy((char*)bytes_data(result) + total_size, bytes_data(arg), arg->len);
		total_size += arg->len;
	}

	return (Object*)result;
}
BUILTIN_METHOD(join, bytes_join, bytes);

/////////////////////////////////////
/// tuple methods
/////////////////////////////////////

Object *tuple_hash(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_tuple)) {
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
	if (!isinstance_inner(args->data[0], &g_tuple )|| !isinstance_inner(args->data[1], &g_tuple)) {
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
	if (!isinstance_inner(args->data[0], &g_tuple)) {
		error = exc_msg(&g_TypeError, "Expected tuple");
		return NULL;
	}
	TupleObject *self = (TupleObject*)args->data[0];
	if (isinstance_inner(args->data[1], &g_int)) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return self->data[index];
	} else if (isinstance_inner(args->data[1], &g_slice)) {
		SliceObject *arg = (SliceObject*)args->data[1];
		size_t start, end;
		if (isinstance_inner(arg->start, &g_int)) {
			start = convert_index(self->len, ((IntObject*)arg->start)->value);
		} else if (isinstance_inner(arg->start, &g_nonetype)) {
			start = 0;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (isinstance_inner(arg->end, &g_int)) {
			end = convert_index(self->len, ((IntObject*)arg->end)->value);
		} else if (isinstance_inner(arg->end, &g_nonetype)) {
			end = self->len;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (start > self->len || end > self->len || end < start) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return (Object*)tuple_raw_ex(self->data + start, end - start, self->header.type);
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
	if (!isinstance_inner(args->data[0], &g_tuple )|| !isinstance_inner(args->data[1], &g_tuple)) {
		error = exc_msg(&g_TypeError, "Expected tuple");
		return NULL;
	}
	TupleObject *self = (TupleObject*)args->data[0];
	TupleObject *other = (TupleObject*)args->data[1];

	// overflow here should be physically impossible
	TupleObject *result = tuple_raw_ex(NULL, self->len + other->len, self->header.type);
	memcpy(result->data, self->data, sizeof(Object*) * self->len);
	memcpy(result->data + self->len, other->data, sizeof(Object*) * other->len);
	return (Object*)result;
}
BUILTIN_METHOD(__add__, tuple_add, tuple);

Object *tuple_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_tuple)) {
		error = exc_msg(&g_TypeError, "Expected tuple");
		return NULL;
	}
	TupleObject *self = (TupleObject*)args->data[0];
	return (Object*)bool_raw(self->len != 0);
}
BUILTIN_METHOD(__bool__, tuple_bool, tuple);

Object *tuple_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_tuple)) {
		error = exc_msg(&g_TypeError, "Expected tuple");
		return NULL;
	}
	TupleObject *self = (TupleObject*)args->data[0];
	TupleObject *converted = tuple_raw(NULL, self->len);
	if (converted == NULL) {
		return NULL;
	}
	gc_root((Object*)converted);
	for (size_t i = 0; i < self->len; i++) {
		converted->data[i] = (Object*)bytes_constructor_inner(self->data[i]);
		if (converted->data[i] == NULL) {
			gc_unroot((Object*)converted);
			return NULL;
		}
	}
	TupleObject *inner_args = tuple_raw(NULL, 2);
	if (inner_args == NULL) {
		gc_unroot((Object*)converted);
		return NULL;
	}
	inner_args->data[0] = (Object*)bytes_unowned_raw(", ", 2, NULL);
	if (inner_args->data[0] == NULL) {
		gc_unroot((Object*)converted);
		return NULL;
	}
	inner_args->data[1] = (Object*)converted;
	gc_unroot((Object*)converted);
	gc_root((Object*)inner_args);
	Object *joined_str = bytes_join(inner_args);
	gc_unroot((Object*)inner_args);
	if (joined_str == NULL) {
		return NULL;
	}
	gc_root((Object*)joined_str);
	Object *result = format_inner("[%s]", joined_str);
	gc_unroot((Object*)joined_str);
	return result;
}
BUILTIN_METHOD(__str__, tuple_str, tuple);

/////////////////////////////////////
/// list methods
/////////////////////////////////////

BUILTIN_METHOD(__hash__, dict_hash, list);

Object *list_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_list )|| !isinstance_inner(args->data[1], &g_list)) {
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
	if (!isinstance_inner(args->data[0], &g_list)) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	if (isinstance_inner(args->data[1], &g_int)) {
		size_t index = convert_index(self->len, ((IntObject*)args->data[1])->value);
		if (index >= self->len) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return self->data[index];
	} else if (isinstance_inner(args->data[1], &g_slice)) {
		SliceObject *arg = (SliceObject*)args->data[1];
		size_t start, end;
		if (isinstance_inner(arg->start, &g_int)) {
			start = convert_index(self->len, ((IntObject*)arg->start)->value);
		} else if (isinstance_inner(arg->start, &g_nonetype)) {
			start = 0;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (isinstance_inner(arg->end, &g_int)) {
			end = convert_index(self->len, ((IntObject*)arg->end)->value);
		} else if (isinstance_inner(arg->end, &g_nonetype)) {
			end = self->len;
		} else {
			error = exc_msg(&g_TypeError, "Expected int or nonetype");
			return NULL;
		}
		if (start > self->len || end > self->len || end < start) {
			error = exc_arg(&g_IndexError, args->data[1]);
			return NULL;
		}
		return (Object*)list_raw_ex(self->data + start, end - start, self->header.type);
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
	if (!isinstance_inner(args->data[0], &g_list)) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	if (isinstance_inner(args->data[1], &g_int)) {
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
	if (!isinstance_inner(args->data[0], &g_list)) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	if (CURRENT_GROUP != self->header.group) {
		error = exc_msg(&g_RuntimeError, "Cannot allocate space in another group");
		return NULL;
	}
	void *other_realloc(void * ptr, size_t newsize, size_t oldsize) { return quota_realloc(ptr, newsize, oldsize, self->header.group); }
	size_t index;
	if (args->len == 3 && isinstance_inner(args->data[2], &g_int)) {
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
		Object **new_data = other_realloc(self->data, sizeof(Object*) * (self->cap + 1) * 2, sizeof(Object*) * self->cap);
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
	if (!isinstance_inner(args->data[0], &g_list)) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	size_t index;
	if (args->len == 2 && isinstance_inner(args->data[1], &g_int)) {
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
	if (!isinstance_inner(args->data[0], &g_list)) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	return (Object*)bool_raw(self->len != 0);
}
BUILTIN_METHOD(__bool__, list_bool, list);

Object *list_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_list)) {
		error = exc_msg(&g_TypeError, "Expected list");
		return NULL;
	}
	ListObject *self = (ListObject*)args->data[0];
	TupleObject *converted = tuple_raw(NULL, self->len);
	if (converted == NULL) {
		return NULL;
	}
	gc_root((Object*)converted);
	for (size_t i = 0; i < self->len; i++) {
		converted->data[i] = (Object*)bytes_constructor_inner(self->data[i]);
		if (converted->data[i] == NULL) {
			gc_unroot((Object*)converted);
			return NULL;
		}
	}
	TupleObject *inner_args = tuple_raw(NULL, 2);
	if (inner_args == NULL) {
		gc_unroot((Object*)converted);
		return NULL;
	}
	inner_args->data[0] = (Object*)bytes_unowned_raw(", ", 2, NULL);
	if (inner_args->data[0] == NULL) {
		gc_unroot((Object*)converted);
		return NULL;
	}
	inner_args->data[1] = (Object*)converted;
	gc_unroot((Object*)converted);
	gc_root((Object*)inner_args);
	Object *joined_str = bytes_join(inner_args);
	gc_unroot((Object*)inner_args);
	if (joined_str == NULL) {
		return NULL;
	}
	gc_root((Object*)joined_str);
	Object *result = format_inner("list([%s])", joined_str);
	gc_unroot((Object*)joined_str);
	return result;
}
BUILTIN_METHOD(__str__, list_str, list);

/////////////////////////////////////
/// int methods
/////////////////////////////////////

Object *int_add(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value + ((IntObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__add__, int_add, int);

Object *int_sub(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value - ((IntObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__sub__, int_sub, int);

Object *int_mul(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value * ((IntObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__mul__, int_mul, int);

Object *int_div(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	int64_t divisor = ((IntObject*)args->data[1])->value;
	if (divisor == 0) {
		error = exc_msg(&g_ZeroDivisionError, "Division by zero");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value / divisor, args->data[0]->type);
}
BUILTIN_METHOD(__div__, int_div, int);

Object *int_mod(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	int64_t divisor = ((IntObject*)args->data[1])->value;
	if (divisor == 0) {
		error = exc_msg(&g_ZeroDivisionError, "Division by zero");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value % divisor, args->data[0]->type);
}
BUILTIN_METHOD(__mod__, int_mod, int);

Object *int_and(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value & ((IntObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__and__, int_and, int);

Object *int_or(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value | ((IntObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__or__, int_or, int);

Object *int_xor(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value ^ ((IntObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__xor__, int_xor, int);

Object *int_shl(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value << ((IntObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__shl__, int_shl, int);

Object *int_shr(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(((IntObject*)args->data[0])->value >> ((IntObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__shr__, int_shr, int);

Object *int_inv(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(~((IntObject*)args->data[0])->value, args->data[0]->type);
}
BUILTIN_METHOD(__inv__, int_inv, int);

Object *int_neg(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	return (Object*)int_raw_ex(-((IntObject*)args->data[0])->value, args->data[0]->type);
}
BUILTIN_METHOD(__neg__, int_neg, int);

Object *int_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
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
	if (!isinstance_inner(args->data[0], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	// return self?
	return (Object*)int_raw(((IntObject*)args->data[0])->value);
}
BUILTIN_METHOD(__hash__, int_hash, int);

Object *int_gt(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
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
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
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
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
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
	if (!isinstance_inner(args->data[0], &g_int )|| !isinstance_inner(args->data[1], &g_int)) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((IntObject*)args->data[0])->value <= ((IntObject*)args->data[1])->value);
}
BUILTIN_METHOD(__le__, int_le, int);

Object *int_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
	}
	if (!isinstance_inner(args->data[0], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	IntObject *self = (IntObject*)args->data[0];
	return (Object*)bool_raw(self->value != 0);
}
BUILTIN_METHOD(__bool__, int_bool, int);

Object *int_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	IntObject *self = (IntObject*)args->data[0];
	char the_str[100];
	snprintf(the_str, 100, "%ld", self->value);
	return (Object*)bytes_raw(the_str, strlen(the_str));
}
BUILTIN_METHOD(__str__, int_str, int);

/////////////////////////////////////
/// bool methods
/////////////////////////////////////

Object *bool_not(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bool)) {
		error = exc_msg(&g_TypeError, "Expected bool");
		return NULL;
	}
	return args->data[0] == (Object*)&g_true ? (Object*)&g_false : (Object*)&g_true;
}
BUILTIN_METHOD(__not__, bool_not, bool);

Object *bool_hash(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bool)) {
		error = exc_msg(&g_TypeError, "Expected bool");
		return NULL;
	}
	return (EmptyObject*)args->data[0] == &g_true ? (Object*)int_raw(1) : (Object*)int_raw(0);
}
BUILTIN_METHOD(__hash__, bool_hash, bool);

Object *bool_bool(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bool)) {
		error = exc_msg(&g_TypeError, "Expected bool");
		return NULL;
	}
	return args->data[0];
}
BUILTIN_METHOD(__bool__, bool_bool, bool);

Object *bool_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_bool)) {
		error = exc_msg(&g_TypeError, "Expected bool");
		return NULL;
	}
	const char *the_str = args->data[0] == (Object*)&g_true ? "True" : "False";
	return (Object*)bytes_unowned_raw(the_str, strlen(the_str), NULL);
}
BUILTIN_METHOD(__str__, bool_str, bool);

/////////////////////////////////////
/// float methods
/////////////////////////////////////

Object *float_add(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_float )|| !isinstance_inner(args->data[1], &g_float)) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	return (Object*)float_raw_ex(((FloatObject*)args->data[0])->value + ((FloatObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__add__, float_add, float);

Object *float_sub(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_float )|| !isinstance_inner(args->data[1], &g_float)) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	return (Object*)float_raw_ex(((FloatObject*)args->data[0])->value - ((FloatObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__sub__, float_sub, float);

Object *float_mul(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_float )|| !isinstance_inner(args->data[1], &g_float)) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	return (Object*)float_raw_ex(((FloatObject*)args->data[0])->value * ((FloatObject*)args->data[1])->value, args->data[0]->type);
}
BUILTIN_METHOD(__mul__, float_mul, float);

Object *float_div(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_float )|| !isinstance_inner(args->data[1], &g_float)) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	double divisor = ((FloatObject*)args->data[1])->value;
	if (divisor == 0) {
		error = exc_msg(&g_ZeroDivisionError, "Division by zero");
		return NULL;
	}
	return (Object*)float_raw_ex(((FloatObject*)args->data[0])->value / divisor, args->data[0]->type);
}
BUILTIN_METHOD(__div__, float_div, float);

Object *float_neg(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_float)) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	return (Object*)float_raw_ex(-((FloatObject*)args->data[0])->value, args->data[0]->type);
}
BUILTIN_METHOD(__neg__, float_neg, float);

Object *float_eq(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_float )|| !isinstance_inner(args->data[1], &g_float)) {
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
	if (!isinstance_inner(args->data[0], &g_float)) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	// return int(self) for integral floats?
	union {
		double *d;
		int64_t *i;
	} x = { .d = &((FloatObject*)args->data[0])->value };
	return (Object*)int_raw(*x.i);
}
BUILTIN_METHOD(__hash__, float_hash, float);

Object *float_gt(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_float )|| !isinstance_inner(args->data[1], &g_float)) {
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
	if (!isinstance_inner(args->data[0], &g_float )|| !isinstance_inner(args->data[1], &g_float)) {
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
	if (!isinstance_inner(args->data[0], &g_float )|| !isinstance_inner(args->data[1], &g_float)) {
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
	if (!isinstance_inner(args->data[0], &g_float )|| !isinstance_inner(args->data[1], &g_float)) {
		return (Object*)bool_raw(false);
	}
	return (Object*)bool_raw(((FloatObject*)args->data[0])->value <= ((FloatObject*)args->data[1])->value);
}
BUILTIN_METHOD(__le__, float_le, float);

Object *float_bool(TupleObject *args) {
	if (args->len != 1 || !isinstance_inner(args->data[0], &g_float)) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	FloatObject *self = (FloatObject*)args->data[0];
	return (Object*)bool_raw(self->value != 0.);
}
BUILTIN_METHOD(__bool__, float_bool, float);

Object *float_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_float)) {
		error = exc_msg(&g_TypeError, "Expected float");
		return NULL;
	}
	FloatObject *self = (FloatObject*)args->data[0];
	char the_str[100];
	snprintf(the_str, 100, "%e", self->value);
	return (Object*)bytes_raw(the_str, strlen(the_str));
}
BUILTIN_METHOD(__str__, float_str, float);

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

Object *object_not(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	EmptyObject *bool_result = bool_constructor_inner(args->data[0]);
	return bool_result == &g_true ? (Object*)&g_false : (Object*)&g_true;
}
BUILTIN_METHOD(__not__, object_not, object);

Object *object_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	return format_inner("<object %s>", int_raw((int64_t)args->data[0]));
}
BUILTIN_METHOD(__str__, object_str, object);

Object *object_repr(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	Object *str_method = get_attr_inner(args->data[0], "__str__");
	if (str_method == NULL) {
		return NULL;
	}
	return call(str_method, args);
}
BUILTIN_METHOD(__repr__, object_repr, object);

Object *object_iter(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	ListIterator *result = (ListIterator*)gc_alloc(sizeof(ListIterator));
	if (result == NULL) {
		return NULL;
	}
	result->header.type = &g_list_iterator;
	result->header.table = &list_iterator_table;
	result->child = args->data[0];
	result->next_index = 0;
	return (Object*)result;
}
BUILTIN_METHOD(__iter__, object_iter, object);

/////////////////////////////////////
/// exception methods
/////////////////////////////////////

Object *exc_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_exception)) {
		error = exc_msg(&g_TypeError, "Expected exception");
		return NULL;
	}
	ExceptionObject *self = (ExceptionObject*)args->data[0];
	TupleObject *inner_args = tuple_raw((Object*[]){(Object*)self->args}, 1);
	if (inner_args == NULL) {
		return NULL;
	}
	gc_root((Object*)inner_args);
	Object *inner_repr = tuple_str(inner_args);
	gc_unroot((Object*)inner_args);
	if (inner_repr == NULL) {
		return NULL;
	}
	gc_root((Object*)inner_repr);
	Object *result = format_inner("Exception%s", inner_repr);
	gc_unroot((Object*)inner_repr);
	return result;
}
BUILTIN_METHOD(__str__, exc_str, exception);

/////////////////////////////////////
/// slice functions
/////////////////////////////////////

Object *slice_str(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_slice)) {
		error = exc_msg(&g_TypeError, "Expected slice");
		return NULL;
	}
	SliceObject *self = (SliceObject*)args->data[0];
	return format_inner("slice(%s, %s)", self->start, self->end);
}
BUILTIN_METHOD(__str__, slice_str, slice);

/////////////////////////////////////
/// nonetype methods
/////////////////////////////////////

Object *none_str(TupleObject *args) {
	return (Object*)bytes_unowned_raw("None", 4, NULL);
}
BUILTIN_METHOD(__str__, none_str, nonetype);

/////////////////////////////////////
/// thread methods
/////////////////////////////////////

Object *thread_str(TupleObject *args) {
	return (Object*)bytes_unowned_raw("<Thread>", 8, NULL);
}
BUILTIN_METHOD(__str__, thread_str, thread);

Object *thread_next(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_thread)) {
		error = exc_msg(&g_TypeError, "Expected thread");
		return NULL;
	}
	ThreadObject *self = (ThreadObject*)args->data[0];

	while (self->status == RUNNING) {
		sleep_inner(0.0000001);
	}
	if (self->status == YIELDED) {
		self->status = RUNNING;
		return self->result;
	}
	if (self->status == RETURNED) {
		error = exc_nil(&g_StopIteration);
		return NULL;
	}
	if (self->status == EXCEPTED) {
		error = self->result;
		return NULL;
	}
	error = exc_msg(&g_RuntimeError, "Thread is in a bad state");
	return NULL;
}
BUILTIN_METHOD(__next__, thread_next, thread);

Object *thread_join(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_thread)) {
		error = exc_msg(&g_TypeError, "Expected thread");
		return NULL;
	}
	ThreadObject *self = (ThreadObject*)args->data[0];

	while (self->status != RETURNED && self->status != EXCEPTED) {
		sleep_inner(0.0000001);
	}

	// TODO timeout
	return (Object*)&g_none;
}
BUILTIN_METHOD(join, thread_join, thread);

Object *thread_inject(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_thread)) {
		error = exc_msg(&g_TypeError, "Expected thread");
		return NULL;
	}
	if (!isinstance_inner(args->data[1], &g_exception)) {
		error = exc_msg(&g_TypeError, "Expected exception");
		return NULL;
	}

	ThreadObject *self = (ThreadObject*)args->data[0];
	self->injected = (ExceptionObject*)args->data[1];
	return (Object*)&g_none;
}
BUILTIN_METHOD(inject, thread_inject, thread);

BUILTIN_METHOD(__iter__, own_iter, thread);

/////////////////////////////////////
/// threadgroup methods
/////////////////////////////////////

Object *threadgroup_str(TupleObject *args) {
	return (Object*)bytes_unowned_raw("<Threadgroup>", 8, NULL);
}
BUILTIN_METHOD(__str__, threadgroup_str, threadgroup);

Object *threadgroup_donate(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_threadgroup)) {
		error = exc_msg(&g_TypeError, "Expected threadgroup");
		return NULL;
	}
	if (isinstance_inner(args->data[1], &g_threadgroup)) {
		error = exc_msg(&g_TypeError, "Do NOT make me think about what this would do @_@");
		return NULL;
	}

	if (CURRENT_GROUP != args->data[1]->group) {
		error = exc_msg(&g_ValueError, "You can't donate an object you don't own!");
		return NULL;
	}
	ThreadGroupObject *new_group = (ThreadGroupObject*)args->data[0];
	size_t the_size = size(args->data[1]);
	if (new_group->mem_used + the_size > new_group->mem_limit) {
		error = (Object*)&MemoryError_inst;
	}

	// lord have mercy
	args->data[1]->group->mem_used -= the_size;
	new_group->mem_used += the_size;
	args->data[1]->group = new_group;
	return (Object*)&g_none;
}
BUILTIN_METHOD(donate, threadgroup_donate, threadgroup);

/////////////////////////////////////
/// freestanding functions
/////////////////////////////////////

Object *builtin_format(TupleObject *args) {
	TupleObject *converted_args = tuple_raw(NULL, args->len);
	if (converted_args == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < args->len; i++) {
		converted_args->data[i] = (Object*)bytes_constructor_inner(args->data[i]);
		if (converted_args->data[i] == NULL) {
			return NULL;
		}
	}
	BytesObject *empty_string = bytes_raw(NULL, 0);
	if (empty_string == NULL) { return NULL; }
	TupleObject *inner_args = tuple_raw(NULL, 2);
	if (inner_args == NULL) { return NULL; }
	inner_args->data[0] = (Object*)empty_string;
	inner_args->data[1] = (Object*)converted_args;
	return bytes_join(inner_args);
}
BUILTIN_FUNCTION(format, builtin_format);

Object *builtin_print(TupleObject *args) {
	Object *formatted = builtin_format(args);
	if (formatted == NULL) {
		return NULL;
	}
	void writer() {
		fwrite(bytes_data((BytesObject*)formatted), 1, ((BytesObject*)formatted)->len, stdout);
		puts("");
	}
	gil_yield(writer);
	return (Object*)&g_none;
}
BUILTIN_FUNCTION(print, builtin_print);

Object *format_inner(const char *format, ...) {
	va_list ap;
	va_start(ap, format);

	size_t num_fmt = 0;
	bool in_const = false;
	for (const char *i = format; *i; i++) {
		if (*i == '%') {
			in_const = false;
			i++;
			if (*i != '%') {
				num_fmt++;
			} else {
				in_const = true;
				num_fmt++;
			}
		} else {
			if (!in_const) {
				in_const = true;
				num_fmt++;
			}
		}
	}
	TupleObject *inner_args = tuple_raw(NULL, num_fmt);
	if (inner_args == NULL) {
		return NULL;
	}

	const char *const_start = NULL;
	size_t idx = 0;
	const char *i;
	for (i = format; *i; i++) {
		if (*i == '%') {
			if (const_start != NULL) {
				inner_args->data[idx] = (Object*)bytes_unowned_raw(const_start, i - const_start, NULL);
				if (inner_args->data[idx] == NULL) {
					return NULL;
				}
				idx++;
			}
			const_start = NULL;
			i++;
			if (*i != '%') {
				inner_args->data[idx] = va_arg(ap, Object*);
				idx++;
			} else {
				const_start = i;
			}
		} else {
			if (const_start == NULL) {
				const_start = i;
			}
		}
	}
	if (const_start != NULL) {
		inner_args->data[idx] = (Object*)bytes_unowned_raw(const_start, i - const_start, NULL);
		if (inner_args->data[idx] == NULL) {
			return NULL;
		}
		idx++;
	}
	if (idx != num_fmt) {
		puts("Something terrible has happened");
		exit(1);
	}

	gc_root((Object*)inner_args);
	Object *result = builtin_format(inner_args);
	gc_unroot((Object*)inner_args);
	return result;
}

Object *builtin_hex(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	IntObject *self = (IntObject*)args->data[0];
	char the_str[100];
	if (self->value != 0) {
		snprintf(the_str, 100, "%#lx", self->value);
	} else {
		strcpy(the_str, "0x0");
	}
	return (Object*)bytes_raw(the_str, strlen(the_str));
}
BUILTIN_FUNCTION(hex, builtin_hex);

Object *builtin_isinstance(TupleObject *args) {
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	return isinstance_inner(args->data[0], (TypeObject*)args->data[1]) ? (Object*)&g_true : (Object*)&g_false;
}
BUILTIN_FUNCTION(isinstance, builtin_isinstance);

bool isinstance_inner(Object *obj, TypeObject *type) {
	for (TypeObject *ptype = obj->type; ptype; ptype = ptype->base_class) {
		if (type == ptype) {
			return true;
		}
	}
	return false;
}

Object *builtin_chr(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	if (!isinstance_inner(args->data[0], &g_int)) {
		error = exc_msg(&g_TypeError, "Expected int");
		return NULL;
	}
	IntObject *self = (IntObject*)args->data[0];
	if (self->value < 0 || self->value > 255) {
		error = exc_msg(&g_ValueError, "value out of range for chr()");
	}
	char ch = (char)self->value;
	return (Object*)bytes_raw(&ch, 1);
}
BUILTIN_FUNCTION(chr, builtin_chr);

Object *builtin_input(TupleObject *args) {
	if (args->len != 0) {
		error = exc_msg(&g_TypeError, "Expected 0 arguments");
		return NULL;
	}
	char buf[1024];
	bool failure;
	void reader() { failure = !fgets(buf, 1024, stdin); }
	gil_yield(reader);
	if (failure) {
		error = exc_msg(&g_RuntimeError, "Could not read from stdin");
		return NULL;
	}
	return (Object*)bytes_raw(buf, strlen(buf));
}
BUILTIN_FUNCTION(input, builtin_input);

void sleep_inner(double time) {
	struct timespec timespec = { (time_t)time, (long)(time * 1000000000.) % 1000000000l };
	void sleeper() {
		while (nanosleep(&timespec, &timespec) < 0);
	}
	gil_yield(sleeper);
}
Object *builtin_sleep(TupleObject *args) {
	if (args->len != 1) {
		error = exc_msg(&g_TypeError, "Expected 1 argument");
		return NULL;
	}
	double sleep;
	if (isinstance_inner(args->data[0], &g_int)) {
		sleep = (double)((IntObject*)args->data[0])->value;
	} else if (isinstance_inner(args->data[0], &g_float)) {
		sleep = ((FloatObject*)args->data[0])->value;
	} else {
		error = exc_msg(&g_TypeError, "Expected int or float");
		return NULL;
	}
	sleep_inner(sleep);
	return (Object*)&g_none;
}
BUILTIN_FUNCTION(sleep, builtin_sleep);
