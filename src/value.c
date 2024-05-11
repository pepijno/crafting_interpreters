#include "value.h"

#include "memory.h"
#include "object.h"

#include <stdio.h>
#include <string.h>

void
init_value_array(struct value_array_t array[static 1]) {
    *array = (struct value_array_t){
        .capacity = 0,
        .count    = 0,
        .values   = nullptr,
    };
}

void
write_value_array(struct value_array_t array[static 1], struct value_t value) {
    if (array->capacity < array->count + 1) {
        i32 oldCapacity = array->capacity;
        array->capacity = grow_capacity(oldCapacity);
        array->values   = grow_array(
            struct value_t, array->values, oldCapacity, array->capacity
        );
    }

    array->values[array->count] = value;
    array->count += 1;
}

void
free_value_array(struct value_array_t array[static 1]) {
    free_array(struct value_t, array->values, array->capacity);
    init_value_array(array);
}

void
print_value(struct value_t value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJECT:
            print_object(value);
            break;
    }
}

bool
values_equal(struct value_t a, struct value_t b) {
    if (a.type != b.type) {
        return false;
    }
    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJECT:
            return AS_OBJECT(a) == AS_OBJECT(b);
        default:
            return false; // Unreachable.
    }
}
