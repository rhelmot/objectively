#pragma once

#include "object.h"

extern __thread Object* oly_error;
#define error oly_error

extern TypeObject g_MemoryError;
extern TypeObject g_RuntimeError;
extern TypeObject g_AttributeError;
extern TypeObject g_KeyError;
extern TypeObject g_IndexError;
extern TypeObject g_TypeError;
extern TypeObject g_ValueError;
extern TypeObject g_ZeroDivisionError;

extern ExceptionObject MemoryError_inst;

Object *exc_msg(TypeObject *type, char *msg);
Object *exc_arg(TypeObject *type, Object *arg);
