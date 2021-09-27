#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "dict.h"

typedef struct ObjectHeader Object;
typedef struct TupleObject TupleObject;
typedef struct TypeObject TypeObject;

typedef struct ObjectTable {
	bool (*trace)(Object *self, bool (*tracer)(Object *tracee));
	void (*finalize)(Object *self);
	Object *(*get_attr)(Object *self, Object *name);
	bool (*set_attr)(Object *self, Object *name, Object *value);
	bool (*del_attr)(Object *self, Object *name);
	Object *(*call)(Object *self, TupleObject *args);
} ObjectTable;

typedef struct ObjectHeader {
	ObjectTable *table;
	TypeObject *type;
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

typedef struct BasicObject {
	DictObject header_dict;
} BasicObject;
bool object_trace(Object *self, bool (*tracer)(Object *tracee));
void object_finalize(Object *self);
Object *object_get_attr(Object *self, Object *name);
bool object_set_attr(Object *self, Object *name, Object *value);
bool object_del_attr(Object *self, Object *name);
Object *object_call(Object *self, TupleObject *args);

typedef struct TypeObject {
	BasicObject header_basic;
	TypeObject *base_class;
	Object *(*constructor)(Object *self, TupleObject *args);
	Object *name;
} TypeObject;
bool type_trace(Object *self, bool (*tracer)(Object *tracee));
Object *type_get_attr(Object *self, Object *name);
Object *type_call(Object *self, TupleObject *args);

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

typedef struct TupleObject {
	ObjectHeader header;
	size_t len;
	Object *data[0];
} TupleObject;
bool tuple_trace(Object *self, bool (*tracer)(Object *tracee));
Object *tuple_get_attr(Object *self, Object *name);

typedef struct BytesObject {
	ObjectHeader header;
	size_t len;
	char _data[0];
} BytesObject;
Object *bytes_get_attr(Object *self, Object *name);

typedef struct BytesUnownedObject {
	BytesObject header_bytes;
	char *_data;
	Object *owner;
} BytesUnownedObject;
bool bytes_unowned_trace(Object *self, bool (*tracer)(Object *tracee));
Object *bytes_unowned_get_attr(Object *self, Object *name);
char *bytes_data(BytesObject *self);

typedef struct BuiltinFunctionObject {
	ObjectHeader header;
	Object * (*func)(TupleObject *args);
} BuiltinFunctionObject;
Object *builtinfunction_call(Object *self, TupleObject *args);

typedef struct ClosureObject {
	ObjectHeader header;
	BytesObject *name;
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

Object *get_attr(Object *self, Object *name);
bool set_attr(Object *self, Object *name, Object *value);
bool del_attr(Object *self, Object *name);
Object *get_attr_inner(Object *self, char *name);
bool set_attr_inner(Object *self, char *name, Object *value);
bool del_attr_inner(Object *self, char *name);
Object *call(Object *method, TupleObject *args);
bool trace(Object *self, bool (*tracer)(Object *tracee));
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

extern EmptyObject g_none;
extern EmptyObject g_true;
extern EmptyObject g_false;

extern ObjectTable builtinfunction_table;

IntObject *int_raw(int64_t value);
EmptyObject *bool_raw(bool value);
FloatObject *float_raw(double value);
ListObject *list_raw(Object **data, size_t len);
TupleObject *tuple_raw(Object **data, size_t len);
BytesObject *bytes_raw(const char *data, size_t len);
BytesUnownedObject *bytes_unowned_raw(char *data, size_t len, Object *owner);
DictObject *dicto_raw();
BasicObject *object_raw(TypeObject *type);
ClosureObject *closure_raw(BytesObject *name, BytesObject *bytecode, DictObject *context);
BoundMethodObject *boundmeth_raw(Object *meth, Object *self);

typedef struct MemberInitEntry {
	DictObject *self;
	const char *name;
	Object *value;
} MemberInitEntry;

#define ADD_MEMBER(cls, s_name, s_value) \
static MemberInitEntry static_add_##cls##_##s_value __attribute((used, section("static_member_adds"))) = { \
	.self = (DictObject*)(&cls), \
	.name = (s_name), \
	.value = (Object*)(&s_value), \
}
