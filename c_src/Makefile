SOURCES = object.c gc.c dict.c main.c builtins.c interpreter.c errors.c
HEADERS = object.h gc.h dict.h builtins.h interpreter.h errors.h
CFLAGS ?= -Wall -g
oly: $(SOURCES) $(HEADERS)
	gcc $(CFLAGS) -o oly $(SOURCES)
clean:
	rm -f oly
