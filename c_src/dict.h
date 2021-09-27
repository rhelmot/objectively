#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct DictChain {
	struct DictChain *next;
	void *key, *val;
} DictChain;
typedef struct DictCore {
	size_t len, cap, generation;
	DictChain *buckets;
} DictCore;

typedef struct EqualityResult {
	bool equals;
	bool success;
} EqualityResult;
typedef struct HashResult {
	uint64_t hash;
	bool success;
} HashResult;
typedef struct GetResult {
	void *val;
	bool found;
	bool success;
} GetResult;

typedef HashResult (*hash_func_t)(void *obj);
typedef EqualityResult (*equality_func_t)(void *obj1, void *obj2);

void dict_construct(DictCore *dict);
void dict_destruct(DictCore *dict);

bool dict_set(DictCore *dict, void *key, void *val, hash_func_t hash_func, equality_func_t equality_func);
GetResult dict_get(DictCore *dict, void *key, hash_func_t hash_func, equality_func_t equality_func);
GetResult dict_pop(DictCore *dict, void *key, hash_func_t hash_func, equality_func_t equality_func);
bool dict_trace(DictCore *dict, bool (*tracer)(void *key, void **val));
bool dict_popwhere(DictCore *dict, bool (*predicate)(void *key, void *val));
