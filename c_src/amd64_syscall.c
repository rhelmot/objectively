#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>

#include "object.h"
#include "errors.h"
#include "thread.h"
#include "builtins.h"

BUILTIN_TYPE(OSError, exception, exc_constructor);

BasicObject g_sys = {
	.header_dict.header = {
		.table = &object_table,
		.type = &g_object,
		.group = &root_threadgroup,
	},
};
STATIC_OBJECT(g_sys);
ADD_MEMBER(builtins, "sys", g_sys);

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define CHECK_C_IDX ({ if (c_idx >= 6) { abort(); } })
#define CHECK_I_IDX ({ if (i_idx >= args->len) { error = exc_msg(&g_TypeError, "Not enough arguments"); return NULL; } })

#define ARG_INT ({ \
	CHECK_I_IDX; \
	if (!isinstance_inner(args->data[i_idx], &g_int)) { \
		error = exc_msg(&g_TypeError, "Expected int"); \
		return NULL; \
	} \
	CHECK_C_IDX; \
	cargs[c_idx] = ((IntObject*)args->data[i_idx])->value; \
	c_idx++; i_idx++; \
})

#define ARG_INBUF_LEN ({ \
	CHECK_I_IDX; \
	if (!isinstance_inner(args->data[i_idx], &g_bytes) && !isinstance_inner(args->data[i_idx], &g_bytearray)) { \
		error = exc_msg(&g_TypeError, "Expected bytes"); \
		return NULL; \
	} \
	BytesObject *b = (BytesObject*)args->data[i_idx]; \
	CHECK_C_IDX; \
	cargs[c_idx] = (uintptr_t)bytes_data(b); \
	c_idx++; CHECK_C_IDX; \
	cargs[c_idx] = b->len; \
	i_idx++; \
})

#define ARG_OUTBUF_LEN ({ \
	CHECK_I_IDX; \
	if (!isinstance_inner(args->data[i_idx], &g_bytearray)) { \
		error = exc_msg(&g_TypeError, "Expected bytearray"); \
		return NULL; \
	} \
	BytesObject *b = (BytesObject*)args->data[i_idx]; \
	CHECK_C_IDX; \
	cargs[c_idx] = (uintptr_t)bytes_data(b); \
	c_idx++; CHECK_C_IDX; \
	cargs[c_idx] = b->len; \
	i_idx++; \
})

#define ARG_INOUTBUF_LEN ({ \
	CHECK_I_IDX; \
	if (!isinstance_inner(args->data[i_idx], &g_bytearray)) { \
		error = exc_msg(&g_TypeError, "Expected bytearray"); \
		return NULL; \
	} \
	BytesObject *b = (BytesObject*)args->data[i_idx]; \
	CHECK_C_IDX; \
	cargs[c_idx] = (uintptr_t)bytes_data(b); \
	c_idx++; CHECK_C_IDX; \
	inoutbuf_data[c_idx] = b->len; \
	cargs_inputs[c_idx] = i_idx; \
	cargs[c_idx] = (uintptr_t)&inoutbuf_data[c_idx]; \
	i_idx++; \
})

#define ARG_STR ({ \
	CHECK_I_IDX; \
	if (!isinstance_inner(args->data[i_idx], &g_bytes) && !isinstance_inner(args->data[i_idx], &g_bytearray)) { \
		error = exc_msg(&g_TypeError, "Expected bytes"); \
	} \
	BytesObject *b = (BytesObject*)args->data[i_idx]; \
	if (memchr(bytes_data(b), '\0', b->len) != NULL) { \
		error = exc_msg(&g_ValueError, "Embedded null byte"); \
		return NULL; \
	} \
	char *cstr = alloca(b->len + 1); /* oh jesus. do allocas live past block scopes? is this ub? */ \
	memcpy(cstr, bytes_data(b), b->len); \
	cstr[b->len] = 0; \
	CHECK_C_IDX; \
	cargs[c_idx] = (uintptr_t)cstr; \
	c_idx++; i_idx++; \
})

#define ARG_TYPEDINBUF(type) ({ \
	CHECK_I_IDX; \
	if (!isinstance_inner(args->data[i_idx], &g_bytes) && !isinstance_inner(args->data[i_idx], &g_bytearray)) { \
		error = exc_msg(&g_TypeError, "Expected bytes"); \
	} \
	BytesObject *b = (BytesObject*)args->data[i_idx]; \
	if (b->len != sizeof(type)) { \
		error = exc_msg(&g_TypeError, #type" requires an argument of size " TOSTRING(sizeof(type))); \
		return NULL; \
	} \
	CHECK_C_IDX; \
	cargs[c_idx] = bytes_data(b); \
	c_idx++; i_idx++; \
})

#define ARG_TYPEDOUTBUF(type) ({ \
	CHECK_I_IDX; \
	if (!isinstance_inner(args->data[i_idx], &g_bytearray)) { \
		error = exc_msg(&g_TypeError, "Expected bytearray"); \
	} \
	BytesObject *b = (BytesObject*)args->data[i_idx]; \
	if (b->len != sizeof(type)) { \
		error = exc_msg(&g_TypeError, #type" requires an argument of size " TOSTRING(sizeof(type))); \
		return NULL; \
	} \
	CHECK_C_IDX; \
	cargs[c_idx] = (uintptr_t)bytes_data(b); \
	c_idx++; i_idx++; \
})

#define RET_INT ({ \
	return (Object*)int_raw(result); \
})

#define SYSCALL(retty, name, margs) \
Object *builtin_sys_##name(TupleObject *args) { \
	size_t c_idx = 0, i_idx = 0; \
	uintptr_t cargs[6] = {0,0,0,0,0,0}; \
	size_t inoutbuf_data[6] = {-1,-1,-1,-1,-1,-1}; \
	size_t cargs_inputs[6] = {-1,-1,-1,-1,-1,-1}; \
	margs; \
	long result; \
	void runner() { result = syscall(SYS_##name, cargs[0], cargs[1], cargs[2], cargs[3], cargs[4], cargs[5]); } \
	gil_yield(runner); \
	for (c_idx = 0; c_idx < 6; c_idx++) { \
		if (inoutbuf_data[c_idx] == -1ul) continue; \
		i_idx = cargs_inputs[c_idx]; \
		BytearrayObject *i_obj = (BytearrayObject*)args->data[i_idx]; \
		if (i_obj->header_bytes.len > inoutbuf_data[c_idx]) { i_obj->header_bytes.len = inoutbuf_data[c_idx]; } \
	} \
	if (result == -1) { \
		error = exc_msg(&g_OSError, strerror(errno)); \
		return NULL; \
	} \
	retty; \
} \
BuiltinFunctionObject g_sys_##name = { \
	.header = { \
		.type = &g_builtin, \
		.table = &builtinfunction_table, \
		.group = &root_threadgroup, \
	}, \
	.func = builtin_sys_##name, \
}; \
STATIC_OBJECT(g_sys_##name); \
ADD_MEMBER(g_sys, #name, g_sys_##name)

#define SYS_LIT(name) \
IntObject g_##name = { \
	.header.type = &g_int, \
	.header.table = &int_table, \
	.header.group = &root_threadgroup, \
	.value = (name), \
}; \
STATIC_OBJECT(g_##name); \
ADD_MEMBER(g_sys, #name, g_##name)

/*#define SYS_STRUCT(type, name, fields) \
Object *builtin_sys_pack_##name(TupleObject *args)
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

SYSCALL(RET_INT, read, (ARG_INT, ARG_OUTBUF_LEN));
SYSCALL(RET_INT, write, (ARG_INT, ARG_INBUF_LEN));
SYSCALL(RET_INT, open, (ARG_STR, ARG_INT, ARG_INT));
SYSCALL(RET_INT, close, (ARG_INT));
SYSCALL(RET_INT, stat, (ARG_STR, ARG_TYPEDOUTBUF(struct stat)));
SYSCALL(RET_INT, fstat, (ARG_INT, ARG_TYPEDOUTBUF(struct stat)));
SYSCALL(RET_INT, lstat, (ARG_STR, ARG_TYPEDOUTBUF(struct stat)));
SYSCALL(RET_INT, lseek, (ARG_INT, ARG_INT, ARG_INT));
SYSCALL(RET_INT, dup, (ARG_INT));
SYSCALL(RET_INT, dup2, (ARG_INT, ARG_INT));
SYSCALL(RET_INT, getpid, );
SYSCALL(RET_INT, socket, (ARG_INT, ARG_INT, ARG_INT));
SYSCALL(RET_INT, connect, (ARG_INT, ARG_INBUF_LEN));
SYSCALL(RET_INT, accept, (ARG_INT, ARG_INOUTBUF_LEN));
SYSCALL(RET_INT, bind, (ARG_INT, ARG_INBUF_LEN));
SYSCALL(RET_INT, listen, (ARG_INT, ARG_INT));
SYSCALL(RET_INT, setsockopt, (ARG_INT, ARG_INT, ARG_INT, ARG_INBUF_LEN));

SYS_LIT(AF_UNIX);
SYS_LIT(AF_LOCAL);
SYS_LIT(AF_INET);
SYS_LIT(AF_PACKET);
SYS_LIT(SOCK_STREAM);
SYS_LIT(SOCK_DGRAM);
SYS_LIT(SOCK_NONBLOCK);
SYS_LIT(SOCK_CLOEXEC);
SYS_LIT(SOL_SOCKET);
SYS_LIT(SO_REUSEADDR);
