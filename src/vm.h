#pragma once

#include "chunk.h"
#include "table.h"

#define STACK_MAX 256

struct vm_t {
    struct chunk_t* chunk;
    u8* ip;
    struct value_t stack[STACK_MAX];
    struct value_t* stack_top;
	struct table_t globals;
	struct table_t strings;
	struct object_t* objects;
};

enum interpret_result_e {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
};

extern struct vm_t vm;

void init_vm();
void free_vm();
enum interpret_result_e interpret(char const* source);
void push(struct value_t value);
struct value_t pop();
