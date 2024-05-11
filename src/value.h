#pragma once

#include "common.h"

typedef struct object_t object_t;
typedef struct object_string_t object_string_t;

enum value_type_e {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJECT,
};

struct value_t {
    enum value_type_e type;
    union {
        bool boolean;
        double number;
        object_t* object;
    } as;
};

#define BOOL_VAL(value)                 \
    ((struct value_t){                  \
        .type = VAL_BOOL,               \
        .as   = { .boolean = (value) }, \
    })
#define NIL_VAL                  \
    ((struct value_t){           \
        .type = VAL_NIL,         \
        .as   = { .number = 0 }, \
    })
#define NUMBER_VAL(value)              \
    ((struct value_t){                 \
        .type = VAL_NUMBER,            \
        .as   = { .number = (value) }, \
    })
#define OBJECT_VAL(obj)                                 \
    ((struct value_t){                                  \
        .type = VAL_OBJECT,                             \
        .as   = { .object = (struct object_t*) (obj) }, \
    })

#define AS_OBJECT(value) ((value).as.object)
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

struct value_array_t {
    i32 capacity;
    i32 count;
    struct value_t* values;
};

bool values_equal(struct value_t a, struct value_t b);
void init_value_array(struct value_array_t array[static 1]);
void write_value_array(
    struct value_array_t array[static 1], struct value_t value
);
void free_value_array(struct value_array_t array[static 1]);

void print_value(struct value_t value);
