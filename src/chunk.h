#pragma once

#include "common.h"
#include "value.h"

enum op_code {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_CLASS,
    OP_METHOD,
    OP_INVOKE,
};

struct chunk {
    i32 count;
    i32 capacity;
    u8* code;
    i32* lines;
    struct value_array constants;
};

void init_chunk(struct chunk chunk[static 1]);
void free_chunk(struct chunk chunk[static 1]);
void write_chunk(struct chunk chunk[static 1], u8 byte, i32 line);

i32 add_constant(struct chunk chunk[static 1], struct value value);
