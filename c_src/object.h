#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "dict.h"

typedef struct ObjectHeader Object;
typedef struct TupleObject TupleObject;
typedef struct TypeObject TypeObject;
typedef struct BytesObject BytesObject;
typedef struct ThreadGroupObject ThreadGroupObject;

typedef struct ObjectTable {
	bool (*trace)(Object *self, bool (*tracer)(Object *tracee));
	void (*finalize)(Object *self);
	Object *(*get_attr)(Object *self, Object *name);
	bool (*set_attr)(Object *self, Object *name, Object *value);
	bool (*del_attr)(Object *self, Object *name);
	Object *(*call)(Object *self, TupleObject *args);
	union { size_t given; size_t (*computed)(Object*); } size;
} ObjectTable;

typedef struct ObjectHeader {
	ObjectTable *table;
	TypeObject *type;
	ThreadGroupObject *group;
} ObjectHeader;

typedef struct EmptyObject {
	ObjectHeader header;
} EmptyObject;
bool null_trace(Object *self, bool (*tracer)(Object *tracee));
void null_finalize(Object *self);
Object *null_get_attr(Object *self, Object *name);
bool null_set_attr(Object *self, Object *name, Object *value);
bool null_del_attr(Object *self, Object *name);
Object *null_call(Object *self, TupleObject *args);

typedef struct DictObject {
	ObjectHeader header;
	DictCore core;
} DictObject;
bool dicto_trace(Object *self, bool (*tracer)(Object *tracee));
void dicto_finalize(Object *self);
Object *dicto_get_attr(Object *self, Object *name);
size_t dicto_size(Object *self);

typedef struct BasicObject {
	DictObject header_dict;
} BasicObject;
bool object_trace(Object *self, bool (*tracer)(Object *tracee));
void object_finalize(Object *self);
Object *object_get_attr(Object *self, Object *name);
bool object_set_attr(Object *self, Object *name, Object *value);
bool object_del_attr(Object *self, Object *name);
Object *object_call(Object *self, TupleObject *args);
size_t object_size(Object *self);

typedef struct TypeObject {
	BasicObject header_basic;
	TypeObject *base_class;
	Object *(*constructor)(Object *self, TupleObject *args);
} TypeObject;
bool type_trace(Object *self, bool (*tracer)(Object *tracee));
Object *type_get_attr(Object *self, Object *name);
Object *type_call(Object *self, TupleObject *args);
size_t type_size(Object *self);

typedef struct IntObject {
	ObjectHeader header;
	int64_t value;
} IntObject;

typedef struct FloatObject {
	ObjectHeader header;
	double value;
} FloatObject;

typedef struct SliceObject {
	ObjectHeader header;
	Object *start, *end;
} SliceObject;
bool slice_trace(Object *self, bool (*tracer)(Object *tracee));
Object *slice_get_attr(Object *self, Object *name);

typedef struct ListObject {
	ObjectHeader header;
	Object **data;
	size_t len, cap;
} ListObject;
bool list_trace(Object *self, bool (*tracer)(Object *tracee));
void list_finalize(Object *self);
Object *list_get_attr(Object *self, Object *name);
size_t list_size(Object *self);

typedef struct TupleObject {
	ObjectHeader header;
	size_t len;
	Object *data[0];
} TupleObject;
bool tuple_trace(Object *self, bool (*tracer)(Object *tracee));
Object *tuple_get_attr(Object *self, Object *name);
size_t tuple_size(Object *self);

typedef struct BytesObject {
	ObjectHeader header;
	size_t len;
	char _data[0];
} BytesObject;
Object *bytes_get_attr(Object *self, Object *name);
size_t bytes_size(Object *self);

typedef struct BytesUnownedObject {
	BytesObject header_bytes;
	const char *_data;
	Object *owner;
} BytesUnownedObject;
bool bytes_unowned_trace(Object *self, bool (*tracer)(Object *tracee));
Object *bytes_unowned_get_attr(Object *self, Object *name);
const char *bytes_data(BytesObject *self);

typedef struct BuiltinFunctionObject {
	ObjectHeader header;
	Object * (*func)(TupleObject *args);
} BuiltinFunctionObject;
Object *builtinfunction_call(Object *self, TupleObject *args);

typedef struct ClosureObject {
	ObjectHeader header;
	BytesObject *bytecode;
	DictObject *context;
} ClosureObject;
bool closure_trace(Object *self, bool (*tracer)(Object *tracee));
Object *closure_get_attr(Object *self, Object *name);
Object *closure_call(Object *self, TupleObject *args);

typedef struct BoundMethodObject {
	ObjectHeader header;
	Object *method;
	Object *self;
} BoundMethodObject;
bool boundmeth_trace(Object *self, bool (*tracer)(Object *tracee));
Object *boundmeth_get_attr(Object *self, Object *name);
Object *boundmeth_call(Object *self, TupleObject *args);

typedef struct ExceptionObject {
	ObjectHeader header;
	TupleObject *args;
} ExceptionObject;
bool exc_trace(Object *self, bool (*tracer)(Object *tracee));
Object *exc_get_attr(Object *self, Object *name);

Object *get_attr(Object *self, Object *name);
bool set_attr(Object *self, Object *name, Object *value);
bool del_attr(Object *self, Object *name);
Object *get_attr_inner(Object *self, char *name);
bool set_attr_inner(Object *self, char *name, Object *value);
bool del_attr_inner(Object *self, char *name);
Object *call(Object *method, TupleObject *args);
bool trace(Object *self, bool (*tracer)(Object *tracee));
size_t size(Object *self);
EqualityResult object_equals(void *_val1, void *_val2);
HashResult object_hasher(void *val);

extern TypeObject g_type;
extern TypeObject g_int;
extern TypeObject g_float;
extern TypeObject g_bool;
extern TypeObject g_bytes;
extern TypeObject g_dict;
extern TypeObject g_list;
extern TypeObject g_tuple;
extern TypeObject g_builtin;
extern TypeObject g_closure;
extern TypeObject g_nonetype;
extern TypeObject g_boundmeth;
extern TypeObject g_slice;
extern TypeObject g_object;
extern TypeObject g_exception;

extern EmptyObject g_none;
extern EmptyObject g_true;
extern EmptyObject g_false;

extern TupleObject empty_tuple;

extern ObjectTable builtinfunction_table;
extern ObjectTable dicto_table;
extern ObjectTable bytes_unowned_table;
extern ObjectTable exc_table;
extern ObjectTable type_table;

IntObject *int_raw(int64_t value);
IntObject *int_raw_ex(int64_t value, TypeObject *type);
EmptyObject *bool_raw(bool value);
FloatObject *float_raw(double value);
FloatObject *float_raw_ex(double value, TypeObject *type);
ListObject *list_raw(Object **data, size_t len);
ListObject *list_raw_ex(Object **data, size_t len, TypeObject *type);
TupleObject *tuple_raw(Object **data, size_t len);
TupleObject *tuple_raw_ex(Object **data, size_t len, TypeObject *type);
BytesObject *bytes_raw(const char *data, size_t len);
BytesObject *bytes_raw_ex(const char *data, size_t len, TypeObject *type);
BytesUnownedObject *bytes_unowned_raw(const char *data, size_t len, Object *owner);
BytesUnownedObject *bytes_unowned_raw_ex(const char *data, size_t len, Object *owner, TypeObject *type);
DictObject *dicto_raw();
DictObject *dicto_raw_ex(TypeObject *type);
BasicObject *object_raw(TypeObject *type);
ClosureObject *closure_raw(BytesObject *bytecode, DictObject *context);
ClosureObject *closure_raw_ex(BytesObject *bytecode, DictObject *context, TypeObject *type);
BoundMethodObject *boundmeth_raw(Object *meth, Object *self);
SliceObject *slice_raw(Object *start, Object *end);
SliceObject *slice_raw_ex(Object *start, Object *end, TypeObject *type);
ExceptionObject *exc_raw(TypeObject *type, TupleObject *args);

Object *exc_constructor(Object *self, TupleObject *args);
Object *dict_constructor(Object *self, TupleObject *args);
Object *type_constructor(Object *self, TupleObject *args);
DictObject *dict_dup_inner(DictObject *self);
BytesObject *bytes_constructor_inner(Object *arg);
EmptyObject *bool_constructor_inner(Object *arg);
Object *null_constructor(Object *self, TupleObject *args);
bool object_equals_str(Object *obj, char *str);

typedef struct MemberInitEntry {
	DictObject *self;
	BytesUnownedObject *name;
	Object *value;
} MemberInitEntry;

#include "gc.h"
#include "builtins.h"
#define INTERNED_STRING(var_name, s_val) \
BytesUnownedObject var_name = { \
	.header_bytes.header = { \
		.table = &bytes_unowned_table, \
		.type = &g_bytes, \
	}, \
	.header_bytes.len = sizeof(s_val) - 1, \
	._data = s_val, \
};\
STATIC_OBJECT(var_name)

#define ADD_MEMBER(cls, s_name, s_value); \
INTERNED_STRING(str_static_add_##cls##_##s_value, s_name); \
static MemberInitEntry static_add_##cls##_##s_value __attribute((used, section("static_member_adds"), aligned(8))) = { \
	.self = (DictObject*)(&cls), \
	.name = (&str_static_add_##cls##_##s_value), \
	.value = (Object*)(&s_value), \
}

#define BUILTIN_TYPE(s_name, base, cons_func) \
Object *(cons_func)(Object *self, TupleObject *args); \
TypeObject g_##s_name = { \
	.header_basic.header_dict.header = { \
		.table = &type_table, \
		.type = &g_type, \
	}, \
	.base_class = &g_##base, \
	.constructor = (cons_func), \
}; \
STATIC_OBJECT(g_##s_name); \
ADD_MEMBER(builtins, #s_name, g_##s_name)
