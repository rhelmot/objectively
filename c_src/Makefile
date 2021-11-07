SOURCES = thread.c object.c gc.c dict.c builtins.c interpreter.c errors.c main.c
HEADERS = thread.h object.h gc.h dict.h builtins.h interpreter.h errors.h
CFLAGS ?= -Wall -g
LFLAGS ?= -lpthread
oly: $(SOURCES) $(HEADERS)
	gcc $(CFLAGS) -o oly $(SOURCES) $(LFLAGS)
clean:
	rm -f oly
