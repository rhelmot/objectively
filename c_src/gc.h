#pragma once

#include <stdlib.h>

#include "object.h"

__attribute__((constructor)) void gc_init();
Object *gc_alloc(size_t size);
void gc_collect();
void gc_probe();
bool gc_root(Object *obj);
bool gc_unroot(Object *obj);

void *quota_alloc(size_t size, ThreadGroupObject *group);
void quota_dealloc(void *ptr, size_t size, ThreadGroupObject *group);
void *quota_realloc(void *ptr, size_t newsize, size_t oldsize, ThreadGroupObject *group);
void *current_thread_alloc(size_t size);
void current_thread_dealloc(void *ptr, size_t size);
void *current_thread_realloc(void *ptr, size_t newsize, size_t oldsize);
void *global_alloc(size_t size);
void global_dealloc(void *ptr, size_t size);

#define STATIC_OBJECT(obj) static Object* static_##obj __attribute((used, section("static_objects"))) = ((Object*)&obj)
