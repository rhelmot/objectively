#pragma once

#include <stdlib.h>

#include "object.h"

__attribute__((constructor)) void gc_init();
Object *gc_malloc(size_t size);
void gc_collect();
void gc_probe();
bool gc_root(Object *obj);
bool gc_unroot(Object *obj);

#define STATIC_OBJECT(obj) static Object* static_##obj __attribute((used, section("static_objects"))) = ((Object*)&obj)
