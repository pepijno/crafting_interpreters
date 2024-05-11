.DEFAULT_GOAL := all

CC := gcc
CFLAGS := -g -std=c2x -Wall -Wextra -Wpedantic

target := main
srcdir := src
depdir := .dep
objdir := .obj

.PHONY: all
all: main directories

sources := $(wildcard $(srcdir)/*.c)
objects := $(patsubst %.c,%.o,$(sources))
depends := $(patsubst %.c,%.d,$(sources))

main: $(objects)
	$(CC) $(CFLAGS) $^ -o $@

directories:
	@mkdir -p $(depdir)
	@mkdir -p $(objdir)

-include $(depends)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

.PHONY: clean
clean:
	rm -rf -- main $(objects) $(depends)
