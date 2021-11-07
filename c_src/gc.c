#include <stdio.h>
#include <stdlib.h>

#include "gc.h"
#include "dict.h"
#include "object.h"
#include "thread.h"

DictCore all_objects;
DictCore roots;

void *quota_alloc(size_t size, ThreadGroupObject *group) {
	if (group->mem_used + size > group->mem_limit) {
		return NULL;
	}
	void *result = calloc(size, 1);
	if (result == NULL) {
		return NULL;
	}
	group->mem_used += size;
	return result;
}

void quota_dealloc(void *ptr, size_t size, ThreadGroupObject *group) {
	group->mem_used -= size;
	free(ptr);
}

void *quota_realloc(void *ptr, size_t newsize, size_t oldsize, ThreadGroupObject *group) {
	if (group->mem_used - oldsize + newsize > group->mem_limit) {
		return NULL;
	}
	void *result = realloc(ptr, newsize);
	if (result == NULL) {
		return NULL;
	}
	group->mem_used += newsize - oldsize;
	return result;
}

void *current_thread_alloc(size_t size) {
	if (oly_thread == NULL) {
		return global_alloc(size);
	}
	return quota_alloc(size, CURRENT_GROUP);
}

void current_thread_dealloc(void *ptr, size_t size) {
	if (oly_thread == NULL) {
		abort();
	}
	quota_dealloc(ptr, size, CURRENT_GROUP);
}

void *current_thread_realloc(void *ptr, size_t newsize, size_t oldsize) {
	if (oly_thread == NULL) {
		abort();
	}
	return quota_realloc(ptr, newsize, oldsize, CURRENT_GROUP);
}

void *global_alloc(size_t size) {
	return calloc(size, 1);
}

void global_dealloc(void *ptr, size_t size) {
	free(ptr);
}

HashResult gc_hasher(void *val) {
	return (HashResult) {
		.hash=((uint64_t)val * 0x1337) ^ ((uint64_t)val * 0xbeef),
		.success=true,
	};
}

EqualityResult gc_equals(void *val1, void *val2) {
	return (EqualityResult) {
		.equals=val1 == val2,
		.success=true,
	};
}

bool gc_add(Object *obj) {
	return dict_set(&all_objects, obj, NULL, gc_hasher, gc_equals, global_alloc, global_dealloc);
}

extern Object *__start_static_objects;
extern Object *__stop_static_objects;
void gc_init() {
	for (Object **iter = &__start_static_objects; iter != &__stop_static_objects; iter++) {
		gc_add(*iter);
		gc_root(*iter);
	}
}

Object *gc_alloc(size_t size) {
	Object *result = current_thread_alloc(size);
	if (!result) {
		return NULL;
	}
	result->table = NULL;
	result->group = CURRENT_GROUP;
	if (!gc_add(result)) {
		current_thread_dealloc(result, size);
		return NULL;
	}
	return result;
}

bool gc_phase1_unmark(void *key, void **val) {
	*val = (void*)0;
	return true;
}

bool gc_phase2_2_mark(Object *obj) {
	if (obj->table == NULL) {
		puts("Fatal error: gc is processing an uninitialized object");
		abort();
	}

	GetResult get = dict_get(&all_objects, obj, gc_hasher, gc_equals);
	if (!get.success) {
		puts("Fatal error: this should never happen");
		abort();
	}
	if (!get.found) {
		puts("Fatal error: gc found an untracked object during tracing");
		abort();
	}
	if (get.val) {
		return true;
	}


	if (!dict_set(&all_objects, obj, (void*)1, gc_hasher, gc_equals, global_alloc, global_dealloc)) {
		puts("Fatal error: gc could not mark object");
		abort();
	}

	return trace(obj, gc_phase2_2_mark);
}

bool gc_phase2_1_mark(void *key, void **val) {
	gc_phase2_2_mark((Object*)key);
	return true;
}

bool gc_phase3_finalize(void *key, void **val) {
	Object *obj = (Object*)key;
	if (!*val) {
		obj->table->finalize(obj);
	}
	return true;
}

bool gc_phase4_dispose(void *key, void *val) {
	Object *object = (Object*)key;
	if (!val) {
		quota_dealloc(object, size(object), object->group);
		return true;
	}
	return false;
}

void gc_collect() {
	// unmark all_objects
	dict_trace(&all_objects, gc_phase1_unmark);
	// mark roots
	dict_trace(&roots, gc_phase2_1_mark);
	// finalize unmarked objects
	dict_trace(&all_objects, gc_phase3_finalize);
	// dispose of unmarked objects
	dict_popwhere(&all_objects, gc_phase4_dispose, global_dealloc);
}

void gc_probe() {
	// TODO uhhhhhhhhhhhhhhh heuristics
	static int gc_counter = 0;
	gc_counter++;
	if (gc_counter == 1000) {
		gc_counter = 0;
		gc_collect();
	}
}

// TODO this is technically not correct - we need to store a mapping from object to *number of roots*
// so that if you root an object twice and unroot it once it's still rooted
// this is notably a problem with the empty tuple
bool gc_root(Object *obj) {
	bool result = dict_set(&roots, obj, NULL, gc_hasher, gc_equals, global_alloc, global_dealloc);
	return result;
}

bool gc_unroot(Object *obj) {
	GetResult result = dict_pop(&roots, obj, gc_hasher, gc_equals, global_dealloc);
	if (!result.success) {
		puts("This should never happen");
		abort();
	}
	return result.found;
}
