#pragma once

#include "object.h"

extern DictObject builtins;

extern BuiltinFunctionObject g_bytes___hash__;
extern BuiltinFunctionObject g_bytes___eq__;

Object *tuple_getitem(TupleObject *args);

Object *list_pop_back_inner(ListObject *self);
bool list_push_back_inner(ListObject *self, Object *item);

Object *dict_getitem(TupleObject *args);
Object *dict_setitem(TupleObject *args);
Object *dict_popitem(TupleObject *args);

Object *format_inner(const char *format, ...);
Object *bytes_join(TupleObject *args);
Object *builtin_print(TupleObject *args);
bool isinstance_inner(Object *obj, TypeObject *type);
void sleep_inner(double time);
bool donate_inner(ThreadGroupObject *new_group, Object *obj);

#define BUILTIN_METHOD(name, function, cls) \
BuiltinFunctionObject g_##cls##_##name = { \
	.header = { \
		.type = &g_builtin, \
		.table = &builtinfunction_table, \
		.group = &root_threadgroup, \
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
		.group = &root_threadgroup, \
	}, \
	.func = function, \
}; \
STATIC_OBJECT(g_##name); \
ADD_MEMBER(builtins, #name, g_##name)

