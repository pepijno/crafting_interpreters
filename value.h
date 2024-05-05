#pragma once

#include "common.h"

typedef double value_t;

struct value_array_t {
	i32 capacity;
	i32 count;
	value_t* values;
};

void init_value_array(struct value_array_t array[static 1]);
void write_value_array(struct value_array_t array[static 1], value_t value);
void free_value_array(struct value_array_t array[static 1]);

void print_value(value_t value);
