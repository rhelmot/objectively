#include "dict.h"
#include "errors.h"

#include <stdlib.h>
#include <stdbool.h>

typedef struct LookupResult {
	// tagged union. contents have meaning if found_any is set; is_first switches fields
	union {
		DictChain *foundFirst;
		DictChain **foundNext;
	} found;
	bool found_any;
	bool is_first;
	bool success;
} LookupResult;

void dict_destruct(DictCore *dict) {
	if (!dict->buckets) {
		return;
	}
	for (size_t bucket = 0; bucket < dict->cap; bucket++) {
		DictChain *next;
		while ((next = dict->buckets[bucket].next)) {
			dict->buckets[bucket].next = next->next;
			free(next);
		}
	}
	free(dict->buckets);
	dict->buckets = NULL;
}

void dict_construct(DictCore *dict) {
	dict->len = 0;
	dict->cap = 0;
	dict->buckets = NULL;
}

LookupResult dict_lookup(DictCore *dict, void *key, hash_func_t hash_func, equality_func_t equality_func) {
	HashResult hash = hash_func(key);
	if (!hash.success) {
		return (LookupResult) {
			.found_any = false,
			.is_first = false,
			.success = false,
		};
	}

	if (dict->cap == 0) {
		return (LookupResult) {
			.found_any = false,
			.is_first = false,
			.success = true,
		};
	}

	size_t bucket = hash.hash % dict->cap;
	if (dict->buckets[bucket].key) {
		EqualityResult eq = equality_func(key, dict->buckets[bucket].key);
		if (!eq.success) {
			return (LookupResult) {
				.found_any = false,
				.is_first = false,
				.success = false,
			};
		}
		if (eq.equals) {
			return (LookupResult) {
				.found.foundFirst = &dict->buckets[bucket],
				.found_any = true,
				.is_first = true,
				.success = true,
			};
		}
		for (DictChain **next = &dict->buckets[bucket].next; *next; next = &(*next)->next) {
			eq = equality_func(key, (*next)->key);
			if (!eq.success) {
				return (LookupResult) {
					.found_any = false,
					.is_first = false,
					.success = false,
				};
			}
			if (eq.equals) {
				return (LookupResult) {
					.found.foundNext = next,
					.found_any = true,
					.is_first = false,
					.success = true,
				};
			}
		}
	}

	return (LookupResult) {
		.found_any = false,
		.is_first = false,
		.success = true,
	};
}

bool dict_expand(DictCore *dict, hash_func_t hash_func) {
	DictCore temp;
	dict_construct(&temp);
	temp.cap = dict->cap * 2 + 3;
	temp.buckets = calloc(sizeof(DictChain), temp.cap);
	if (!temp.buckets) {
		return false;
	}

	for (size_t bucket = 0; bucket < dict->cap; bucket++) {
		for (DictChain *cur = &dict->buckets[bucket]; cur && cur->key; cur = cur->next) {
			if (!dict_set(&temp, cur->key, cur->val, hash_func, NULL)) {
				dict_destruct(&temp);
				return false;
			}
		}
	}

	// who needs c++?
	dict_destruct(dict);
	*dict = temp;
	return true;
}

bool dict_set(DictCore *dict, void *key, void *val, hash_func_t hash_func, equality_func_t equality_func) {
	if (dict->len >= dict->cap) {
		if (!dict_expand(dict, hash_func)) {
			return false;
		}
	}

	if (equality_func != NULL) {
		LookupResult lookup = dict_lookup(dict, key, hash_func, equality_func);
		if (!lookup.success) {
			return false;
		}
		if (lookup.found_any) {
			if (lookup.is_first) {
				lookup.found.foundFirst->key = key;
				lookup.found.foundFirst->val = val;
			} else {
				(*lookup.found.foundNext)->key = key;
				(*lookup.found.foundNext)->val = val;
			}
			return true;
		}
	}

	HashResult hash = hash_func(key);
	if (!hash.success) {
		return false;
	}
	size_t bucket = hash.hash % dict->cap;
	if (!dict->buckets[bucket].key) {
		dict->buckets[bucket].key = key;
		dict->buckets[bucket].val = val;
		dict->len++;
		dict->generation++;
		return true;
	}

	DictChain **link;
	for (link = &dict->buckets[bucket].next; *link; link = &(*link)->next);
	*link = malloc(sizeof(DictChain));
	if (!*link) {
		error = (Object*)&MemoryError_inst;
		return false;
	}
	(*link)->key = key;
	(*link)->val = val;
	(*link)->next = NULL;
	dict->len++;
	dict->generation++;
	return true;
}

GetResult dict_get(DictCore *dict, void *key, hash_func_t hash_func, equality_func_t equality_func) {
	LookupResult lookup = dict_lookup(dict, key, hash_func, equality_func);
	if (!lookup.success) {
		return (GetResult) {
			.val = NULL,
			.found = false,
			.success = false,
		};
	}
	if (!lookup.found_any) {
		return (GetResult) {
			.val = NULL,
			.found = false,
			.success = true,
		};
	}
	return (GetResult) {
		.val = lookup.is_first ? lookup.found.foundFirst->val : (*lookup.found.foundNext)->val,
		.found = true,
		.success = true,
	};
}

void lookup_pop(LookupResult *lookup) {
	if (lookup->is_first) {
		if (lookup->found.foundFirst->next) {
			DictChain *save = lookup->found.foundFirst->next;
			*lookup->found.foundFirst = *save;
			free(save);
		} else {
			lookup->found.foundFirst->key = NULL;
			lookup->found.foundFirst->val = NULL;
			lookup->found_any = false;
		}
	} else {
		DictChain *save = *lookup->found.foundNext;
		*lookup->found.foundNext = save->next;
		lookup->found_any = *lookup->found.foundNext != NULL;
		free(save);
	}
}

void lookup_next(LookupResult *lookup) {
	if (lookup->is_first) {
		lookup->is_first = false;
		lookup->found.foundNext = &lookup->found.foundFirst->next;
	} else {
		lookup->found.foundNext = &(*lookup->found.foundNext)->next;
	}
	lookup->found_any = *lookup->found.foundNext;
}

void *lookup_key(LookupResult *lookup) {
	if (lookup->is_first) {
		return lookup->found.foundFirst->key;
	} else {
		return (*lookup->found.foundNext)->key;
	}
}

void *lookup_val(LookupResult *lookup) {
	if (lookup->is_first) {
		return lookup->found.foundFirst->val;
	} else {
		return (*lookup->found.foundNext)->val;
	}
}

GetResult dict_pop(DictCore *dict, void *key, hash_func_t hash_func, equality_func_t equality_func) {
	LookupResult lookup = dict_lookup(dict, key, hash_func, equality_func);
	if (!lookup.success) {
		return (GetResult) {
			.val = NULL,
			.found = false,
			.success = false,
		};
	}
	if (!lookup.found_any) {
		return (GetResult) {
			.val = NULL,
			.found = false,
			.success = true,
		};
	}
	GetResult result = {
		.val = lookup.is_first ? lookup.found.foundFirst->val : (*lookup.found.foundNext)->val,
		.found = true,
		.success = true,
	};
	lookup_pop(&lookup);

	dict->len--;
	dict->generation++;
	return result;
}

bool dict_trace(DictCore *dict, bool (*tracer)(void *key, void **val)) {
	size_t generation = dict->generation;
	for (size_t bucket = 0; bucket < dict->cap; bucket++) {
		for (DictChain *cur = &dict->buckets[bucket]; cur && cur->key; cur = cur->next) {
			if (generation != dict->generation) {
				error = exc_msg(&g_RuntimeError, "Dict was modified during iteration");
				return false;
			}
			if (!tracer(cur->key, &cur->val)) {
				return false;
			}
		}
	}
	return true;
}

bool dict_popwhere(DictCore *dict, bool (*predicate)(void *key, void *val)) {
	for (size_t bucket = 0; bucket < dict->cap; bucket++) {
		LookupResult iter = {
			.found.foundFirst = &dict->buckets[bucket],
			.found_any = dict->buckets[bucket].key != 0,
			.is_first = true,
			.success = true,
		};
		while (iter.found_any) {
			size_t generation = dict->generation;
			bool check = predicate(lookup_key(&iter), lookup_val(&iter));
			if (dict->generation != generation) {
				error = exc_msg(&g_RuntimeError, "Dict was modified during iteration");
				return false;
			}
			if (check) {
				lookup_pop(&iter);
				dict->len--;
				dict->generation++;
			} else {
				lookup_next(&iter);
			}
		}
	}
	return true;
}
