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
