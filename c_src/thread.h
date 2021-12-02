#pragma once

#include "object.h"

__attribute__((constructor)) void threads_init();


typedef enum ThreadStatus {
	RUNNING,
	YIELDED,
	RETURNED,
	EXCEPTED,
} ThreadStatus;

typedef struct ThreadGroupObject ThreadGroupObject;

typedef struct ThreadObject {
	ObjectHeader header;
	pthread_t tid;
	Object *target;
	TupleObject *args;
	ThreadStatus status;
	Object *result;
	ExceptionObject *injected;
} ThreadObject;

typedef struct ThreadGroupObject {
	ObjectHeader header;
	uint64_t mem_limit;
	uint64_t mem_used;
	uint64_t yield_interval; // could be a time interval in the future
	ExceptionObject *injected;
} ThreadGroupObject;

extern TypeObject g_thread;
extern TypeObject g_threadgroup;
extern ThreadObject root_thread;
extern ThreadGroupObject root_threadgroup;
bool thread_trace(Object *self, bool (*tracer)(Object *tracee));
Object *thread_get_attr(Object *self, Object *name);
Object* thread_constructor(Object *self, TupleObject *args);
ThreadObject *thread_raw(Object *target, TupleObject *args, TypeObject *type, ThreadGroupObject *group);
void threadgroup_finalize(Object *self);
Object* threadgroup_constructor(Object *self, TupleObject *args);

void gil_yield(void (*sleeper)());
bool gil_probe();
bool thread_yield(Object *val);

extern __thread ThreadObject *oly_thread;
#define CURRENT_GROUP (oly_thread->header.group)
#define CURRENT_INJECTED (oly_thread->injected ? oly_thread->injected : CURRENT_GROUP->injected)
