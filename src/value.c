#include "value.h"

#include "memory.h"
#include "object.h"

#include <stdio.h>

void
init_value_array(struct value_array array[static 1]) {
    *array = (struct value_array){
        .capacity = 0,
        .count    = 0,
        .values   = nullptr,
    };
}

void
write_value_array(struct value_array array[static 1], struct value value) {
    if (array->capacity < array->count + 1) {
        i32 oldCapacity = array->capacity;
        array->capacity = grow_capacity(oldCapacity);
        array->values   = grow_array(
            struct value, array->values, oldCapacity, array->capacity
        );
    }

    array->values[array->count] = value;
    array->count += 1;
}

void
free_value_array(struct value_array array[static 1]) {
    free_array(struct value, array->values, array->capacity);
    init_value_array(array);
}

void
print_value(struct value value) {
#ifdef NAN_BOXING
    if (IS_BOOL(value)) {
        printf(AS_BOOL(value) ? "true" : "false");
    } else if (IS_NIL(value)) {
        printf("nil");
    } else if (IS_NUMBER(value)) {
        printf("%g", AS_NUMBER(value));
    } else if (IS_OBJECT(value)) {
        print_object(value);
    }
#else
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
#endif
}

bool
values_equal(struct value a, struct value b) {
#ifdef NAN_BOXING
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    return a.value == b.value;
#else
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
#endif
}
