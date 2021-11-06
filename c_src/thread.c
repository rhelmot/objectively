#include <pthread.h>

#include "thread.h"
#include "errors.h"
#include "interpreter.h"
#include "builtins.h"

pthread_mutex_t gil;
__thread ThreadObject *oly_thread;

ObjectTable thread_table = {
	.trace = thread_trace,
	.finalize = null_finalize,
	.get_attr = thread_get_attr,
	.set_attr = null_set_attr,
	.del_attr = null_del_attr,
	.call = null_call,
};

BUILTIN_TYPE(thread, object, thread_constructor);


void gil_release() {
	pthread_mutex_unlock(&gil);
}

void gil_acquire() {
	pthread_mutex_lock(&gil);
}

__thread int yield_probe_counter = 0;

void gil_yield(void (*sleeper)()) {
	gil_release();
	sleeper();
	gil_acquire();
}

bool gil_probe() {
	yield_probe_counter++;
	if (yield_probe_counter > 1000) {
		yield_probe_counter = 0;
		sleep_inner(0.0000001);
	}

	if (oly_thread != NULL && oly_thread->injected != NULL) {
		error = (Object*)oly_thread->injected;
		oly_thread->injected = NULL;
		return false;
	}

	return true;
}

void *thread_target(void *_thread) {
	ThreadObject *thread = _thread;
	oly_thread = thread;
	gil_acquire();

	Object *result = call(thread->target, thread->args);
	if (result == NULL) {
		thread->status = EXCEPTED;
		thread->result = error;
	} else {
		thread->status = RETURNED;
		thread->result = result;
	}

	gc_unroot((Object*)thread);
	gil_release();
	return NULL;
}

ThreadObject *thread_raw(Object *target, TupleObject *args, TypeObject *type) {
	ThreadObject *thread = (ThreadObject*)gc_malloc(sizeof(ThreadObject));
	if (thread == NULL) {
		return NULL;
	}
	thread->header.type = type;
	thread->header.table = &thread_table;
	// tid
	thread->target = target;
	thread->args = args;
	thread->status = RUNNING;
	thread->result = NULL;

	gc_root((Object*)thread);

	if (pthread_create(&thread->tid, NULL, thread_target, thread) != 0) {
		gc_unroot((Object*)thread);
		error = (Object*)&MemoryError_inst;
		return NULL;
	}

	return thread;
}

bool thread_yield(Object *val) {
	if (oly_thread == NULL) {
		error = exc_msg(&g_RuntimeError, "Cannot yield from main thread");
		return false;
	}
	if (oly_thread->injected != NULL) {
		error = (Object*)oly_thread->injected;
		oly_thread->injected = NULL;
		return false;
	}
	oly_thread->status = YIELDED;
	oly_thread->result = val;
	while (oly_thread->status == YIELDED) {
		sleep_inner(0.0000001);
	}
	return true;
}

bool thread_trace(Object *_self, bool (*tracer)(Object *tracee)) {
	ThreadObject *self = (ThreadObject*)_self;
	if (!tracer((Object*)self->target)) return false;
	if (!tracer((Object*)self->args)) return false;
	if (self->result != NULL) {
		if (!tracer(self->result)) return false;
	}
	if (self->injected != NULL) {
		if (!tracer(self->injected)) return false;
	}
	return true;
}

Object *thread_get_attr(Object *_self, Object *name) {
	ThreadObject *self = (ThreadObject*)_self;

	if (object_equals_str(name, "status")) {
		return (Object*)int_raw(self->status);
	}
	if (object_equals_str(name, "result")) {
		return self->result;
	}
	error = exc_arg(&g_AttributeError, name);
	return NULL;
}

Object* thread_constructor(Object *_self, TupleObject *args) {
	TypeObject *self = (TypeObject*)_self;
	if (args->len != 2) {
		error = exc_msg(&g_TypeError, "Expected 2 arguments");
		return NULL;
	}
	if (!isinstance_inner(args->data[1], &g_tuple)) {
		error = exc_msg(&g_TypeError, "Argument 2: expected tuple");
		return NULL;
	}

	return (Object*)thread_raw(args->data[0], (TupleObject*)args->data[1], self);
}

ThreadObject main_thread;

void threads_init() {
	pthread_mutex_init(&gil, NULL);
	gil_acquire();
}
