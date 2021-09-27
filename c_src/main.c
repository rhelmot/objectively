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

	ClosureObject *real_main = closure_raw(bytes_raw("__main__", 8), bytes, dicto_raw());
	IntObject /*lol*/ *result = (IntObject*)call((Object*)real_main, argv_obj);
	return result->value;
}
