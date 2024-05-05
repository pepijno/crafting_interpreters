.DEFAULT_GOAL := all

CC := gcc
CFLAGS := -g -std=c2x -Wall -Wextra -Wpedantic

.PHONY: all
all: main

objects := \
	chunk.o \
	debug.o \
	main.o \
	memory.o \
	value.o

main: $(objects)
	$(CC) -o $@ $(objects)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	@rm -rf -- main $(objects)
