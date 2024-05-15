#pragma once

#include "chunk.h"
#include "common.h"
#include "value.h"

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_STRING(value)   is_object_type(value, OBJECT_STRING)
#define IS_FUNCTION(value) is_object_type(value, OBJECT_FUNCTION)
#define IS_NATIVE(value)   is_object_type(value, OBJECT_NATIVE)
#define IS_CLOSURE(value)  is_object_type(value, OBJECT_CLOSURE)

#define AS_STRING(value)   ((struct object_string*) AS_OBJECT(value))
#define AS_CSTRING(value)  (((struct object_string*) AS_OBJECT(value))->chars)
#define AS_FUNCTION(value) ((struct object_function*) AS_OBJECT(value))
#define AS_NATIVE(value)   (((struct object_native*) AS_OBJECT(value))->function)
#define AS_CLOSURE(value)  ((struct object_closure*) AS_OBJECT(value))

enum object_type {
    OBJECT_STRING,
    OBJECT_FUNCTION,
    OBJECT_NATIVE,
    OBJECT_CLOSURE,
    OBJECT_UPVALUE,
};

enum function_type {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
};

struct object {
    enum object_type type;
    bool is_marked;
    struct object* next;
};

struct object_function {
    struct object object;
    i32 arity;
    i32 upvalue_count;
    struct chunk chunk;
    struct object_string* name;
};

typedef struct value (*native_function)(i32 arg_count, struct value* args);

struct object_native {
    struct object object;
    native_function function;
};

struct object_string {
    struct object object;
    i32 length;
    char* chars;
    u32 hash;
};

struct object_upvalue {
    struct object object;
    struct value* location;
    struct value closed;
    struct object_upvalue* next;
};

struct object_closure {
    struct object object;
    struct object_function* function;
    struct object_upvalue** upvalues;
    i32 upvalue_count;
};

struct object_closure* new_closure(struct object_function function[static 1]);
struct object_function* new_function();
struct object_native* new_native(native_function function);
struct object_string* take_string(char* chars, i32 length);
struct object_string* copy_string(char const* chars, i32 length);
struct object_upvalue* new_upvalue(struct value slot[static 1]);

void print_object(struct value value);

static inline bool
is_object_type(struct value value, enum object_type type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}
