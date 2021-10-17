#include <stdio.h>
#include <stdlib.h>

#include "gc.h"
#include "dict.h"
#include "object.h"

DictCore all_objects;
DictCore roots;

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
	return dict_set(&all_objects, obj, NULL, gc_hasher, gc_equals);
}

extern Object *__start_static_objects;
extern Object *__stop_static_objects;
void gc_init() {
	for (Object **iter = &__start_static_objects; iter != &__stop_static_objects; iter++) {
		gc_add(*iter);
		gc_root(*iter);
	}
}

Object *gc_malloc(size_t size) {
	Object *result = calloc(size, 1);
	if (!result) {
		return NULL;
	}
	result->table = NULL;
	if (!gc_add(result)) {
		free(result);
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


	if (!dict_set(&all_objects, obj, (void*)1, gc_hasher, gc_equals)) {
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
	if (!val) {
		free(key);
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
	dict_popwhere(&all_objects, gc_phase4_dispose);
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

bool gc_root(Object *obj) {
	return dict_set(&roots, obj, NULL, gc_hasher, gc_equals);
}

bool gc_unroot(Object *obj) {
	GetResult result = dict_pop(&roots, obj, gc_hasher, gc_equals);
	if (!result.success) {
		puts("This should never happen");
		abort();
	}
	return result.found;
}
