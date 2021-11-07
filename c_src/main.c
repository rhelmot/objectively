#include <stdio.h>
#include <string.h>
#include <alloca.h>

#include "builtins.h"
#include "errors.h"
#include "object.h"
#include "gc.h"
#include "thread.h"

extern DictCore all_objects, roots;
HashResult gc_hasher(void *val);
EqualityResult gc_equals(void *val1, void *val2);

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s src.olc\n", argv[0]);
		return 1;
	}

	FILE *fp = fopen(argv[1], "r");
	if (fp == NULL) {
		printf("File not found: %s\n", argv[1]);
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	size_t filesize = ftell(fp);
	rewind(fp);

	char *data = malloc(filesize);
	if (fread(data, 1, filesize, fp) != filesize) {
		puts("Couldn't read entire file");
		return 1;
	}
	fclose(fp);
	BytesObject *bytes = bytes_raw(data, filesize);
	free(data);

	Object **argv_converted = alloca(sizeof(Object*) * (argc - 2));
	for (int i = 0; i < argc - 2; i++) {
		argv_converted[i] = (Object*)bytes_raw(argv[i + 2], strlen(argv[i + 2]));
	}
	TupleObject *argv_obj = tuple_raw(argv_converted, argc - 2);
	ClosureObject *real_main = closure_raw(bytes, &builtins);

	gc_root((Object*)real_main);
	gc_root((Object*)argv_obj);
	Object *result = call((Object*)real_main, argv_obj);
	gc_unroot((Object*)real_main);
	gc_unroot((Object*)argv_obj);

	int retcode;
	if (result == NULL) {
		puts("Program aborted with error");
		TupleObject *print_args = tuple_raw((Object*[]){error}, 1);
		if (print_args == NULL) {
			puts("Could not convert error to string?");
			return 1;
		}
		gc_root((Object*)print_args);
		if (builtin_print(print_args) == NULL) {
			puts("Could not convert error to string");
		}
		gc_unroot((Object*)print_args);
		retcode = 1;
	} else {
		retcode = result->type == &g_int ? ((IntObject*)result)->value : 0;
	}
	gc_collect();
	if (root_threadgroup.mem_used != 0) {
		printf("remaining: %ld\n", root_threadgroup.mem_used);
		//bool tracer(void *key, void **val) {
		//	if (!dict_get(&roots, key, gc_hasher, gc_equals).found) {
		//		printf("%p\n", key);
		//	}
		//	return true;
		//}
		//dict_trace(&all_objects, tracer);
		//abort();
	}
	return retcode;
}
