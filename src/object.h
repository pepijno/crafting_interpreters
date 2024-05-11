#pragma once

#include "common.h"
#include "value.h"

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_STRING(value) is_object_type(value, OBJECT_STRING)

#define AS_STRING(value)  ((struct object_string_t*) AS_OBJECT(value))
#define AS_CSTRING(value) (((struct object_string_t*) AS_OBJECT(value))->chars)

enum object_type_e {
    OBJECT_STRING,
};

struct object_t {
    enum object_type_e type;
    struct object_t* next;
};

struct object_string_t {
    struct object_t object;
    i32 length;
    char* chars;
    u32 hash;
};

struct object_string_t* take_string(char* chars, i32 length);
struct object_string_t* copy_string(char const* chars, i32 length);

void print_object(struct value_t value);

static inline bool
is_object_type(struct value_t value, enum object_type_e type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}
