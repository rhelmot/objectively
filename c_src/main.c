#include <stdio.h>
#include <string.h>
#include <alloca.h>

#include "object.h"
#include "gc.h"

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
	fread(data, 1, filesize, fp);
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
	IntObject /*lol*/ *result = (IntObject*)call((Object*)real_main, argv_obj);
	gc_unroot((Object*)real_main);
	gc_unroot((Object*)argv_obj);

	if (result == NULL) {
		puts("Program aborted with error");
		gc_collect();
		return 1;
	} else {
		gc_collect();
		return result->value;
	}
}
