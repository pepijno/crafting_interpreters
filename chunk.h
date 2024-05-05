#pragma once

#include "common.h"
#include "value.h"

enum op_code_e {
	OP_CONSTANT,
	OP_RETURN,
};

struct chunk_t {
	i32 count;
	i32 capacity;
	u8* code;
	i32* lines;
	struct value_array_t constants;
};

void init_chunk(struct chunk_t chunk[static 1]);
void free_chunk(struct chunk_t chunk[static 1]);
void write_chunk(struct chunk_t chunk[static 1], u8 byte, i32 line);

i32 add_constant(struct chunk_t chunk[static 1], value_t value);
