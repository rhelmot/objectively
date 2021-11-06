#pragma once

#include "object.h"

__attribute__((constructor)) void threads_init();


typedef enum ThreadStatus {
	RUNNING,
	YIELDED,
	RETURNED,
	EXCEPTED,
} ThreadStatus;

typedef struct ThreadObject {
	ObjectHeader header;
	pthread_t tid;
	Object *target;
	TupleObject *args;
	ThreadStatus status;
	Object *result;
	ExceptionObject *injected;
} ThreadObject;

extern TypeObject g_thread;
bool thread_trace(Object *self, bool (*tracer)(Object *tracee));
Object *thread_get_attr(Object *self, Object *name);
Object* thread_constructor(Object *self, TupleObject *args);
ThreadObject *thread_raw(Object *target, TupleObject *args, TypeObject *type);

void gil_yield(void (*sleeper)());
bool gil_probe();
bool thread_yield(Object *val);

extern __thread ThreadObject *oly_thread;
