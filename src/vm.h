#pragma once

#include "object.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

struct call_frame {
    struct object_function* function;
    u8* ip;
    struct value* slots;
};

struct vm {
    struct call_frame frames[FRAMES_MAX];
    i32 frame_count;
    struct value stack[STACK_MAX];
    struct value* stack_top;
    struct table globals;
    struct table strings;
    struct object* objects;
};

enum interpret_result {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
};

extern struct vm vm;

void init_vm();
void free_vm();
enum interpret_result interpret(char const* source);
void push(struct value value);
struct value pop();
