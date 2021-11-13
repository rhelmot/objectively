#include <string.h>

#include "errors.h"
#include "object.h"

__thread Object* error;

BUILTIN_TYPE(MemoryError, exception, exc_constructor);
BUILTIN_TYPE(RuntimeError, exception, exc_constructor);
BUILTIN_TYPE(AttributeError, exception, exc_constructor);
BUILTIN_TYPE(KeyError, exception, exc_constructor);
BUILTIN_TYPE(IndexError, exception, exc_constructor);
BUILTIN_TYPE(TypeError, exception, exc_constructor);
BUILTIN_TYPE(ValueError, exception, exc_constructor);
BUILTIN_TYPE(ZeroDivisionError, exception, exc_constructor);
BUILTIN_TYPE(StopIteration, exception, exc_constructor);
BUILTIN_TYPE(Cancellation, exception, exc_constructor);

ExceptionObject MemoryError_inst = {
	.header = {
		.type = &g_MemoryError,
		.table = &exc_table,
	},
	.args = &empty_tuple,
};
STATIC_OBJECT(MemoryError_inst);

Object *exc_msg(TypeObject *type, char *msg) {
	BytesObject *msg_obj = bytes_raw(msg, strlen(msg));
	if (msg_obj == NULL) {
		return (Object*)&MemoryError_inst;
	}
	TupleObject *args = tuple_raw((Object**)&msg_obj, 1);
	if (args == NULL) {
		return (Object*)&MemoryError_inst;
	}
	ExceptionObject *result = exc_raw(type, args);
	if (result == NULL) {
		return (Object*)&MemoryError_inst;
	}
	return (Object*)result;
}

Object *exc_arg(TypeObject *type, Object *arg) {
	TupleObject *args = tuple_raw(&arg, 1);
	if (args == NULL) {
		return (Object*)&MemoryError_inst;
	}
	ExceptionObject *result = exc_raw(type, args);
	if (result == NULL) {
		return (Object*)&MemoryError_inst;
	}
	return (Object*)result;
}

Object *exc_nil(TypeObject *type) {
	TupleObject *args = tuple_raw(NULL, 0);
	if (args == NULL) {
		return (Object*)&MemoryError_inst;
	}
	ExceptionObject *result = exc_raw(type, args);
	if (result == NULL) {
		return (Object*)&MemoryError_inst;
	}
	return (Object*)result;
}
