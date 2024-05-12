#pragma once

#include "common.h"

typedef struct object object;
typedef struct object_string object_string;

enum value_type {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJECT,
};

struct value {
    enum value_type type;
    union {
        bool boolean;
        double number;
        object* object;
    } as;
};

#define BOOL_VAL(val)                 \
    ((struct value){                  \
        .type = VAL_BOOL,             \
        .as   = { .boolean = (val) }, \
    })
#define NIL_VAL                  \
    ((struct value){             \
        .type = VAL_NIL,         \
        .as   = { .number = 0 }, \
    })
#define NUMBER_VAL(val)              \
    ((struct value){                 \
        .type = VAL_NUMBER,          \
        .as   = { .number = (val) }, \
    })
#define OBJECT_VAL(obj)                               \
    ((struct value){                                  \
        .type = VAL_OBJECT,                           \
        .as   = { .object = (struct object*) (obj) }, \
    })

#define AS_OBJECT(value) ((value).as.object)
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

struct value_array {
    i32 capacity;
    i32 count;
    struct value* values;
};

bool values_equal(struct value a, struct value b);
void init_value_array(struct value_array array[static 1]);
void write_value_array(struct value_array array[static 1], struct value value);
void free_value_array(struct value_array array[static 1]);

void print_value(struct value value);
