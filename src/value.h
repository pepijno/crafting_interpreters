#pragma once

#include "common.h"

#include <string.h>

typedef struct object object;
typedef struct object_string object_string;

enum value_type {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJECT,
};

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t) 0x8000000000000000)
#define QNAN     ((uint64_t) 0x7ffc000000000000)

#define TAG_NIL   1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE  3 // 11.

struct value {
    uint64_t value;
};

#define IS_BOOL(val)   (((val).value | 1) == TRUE_VAL.value)
#define IS_NIL(val)    ((val).value == NIL_VAL.value)
#define IS_NUMBER(val) (((val).value & QNAN) != QNAN)
#define IS_OBJECT(val) (((val).value & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(val)   ((val).value == TRUE_VAL.value)
#define AS_NUMBER(val) value_to_num(val)
#define AS_OBJECT(val) \
    ((struct object*) (uintptr_t) (((val).value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((struct value){ (uint64_t) (QNAN | TAG_FALSE) })
#define TRUE_VAL        ((struct value){ (uint64_t) (QNAN | TAG_TRUE) })
#define NIL_VAL         ((struct value){ (uint64_t) (QNAN | TAG_NIL) })
#define NUMBER_VAL(num) num_to_value(num)
#define OBJECT_VAL(obj) \
    ((struct value){ (SIGN_BIT | QNAN | (uint64_t) (uintptr_t) (obj)) })

static inline double
value_to_num(struct value value) {
    double num;
    memcpy(&num, &value.value, sizeof(uint64_t));
    return num;
}

static inline struct value
num_to_value(double num) {
    struct value value;
    memcpy(&value.value, &num, sizeof(double));
    return value;
}

#else

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

#endif

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
