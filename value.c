#include "value.h"

#include "memory.h"

#include <stdio.h>

void
init_value_array(struct value_array_t array[static 1]) {
    *array = (struct value_array_t){
        .capacity = 0,
        .count    = 0,
        .values   = nullptr,
    };
}

void
write_value_array(struct value_array_t array[static 1], value_t value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = grow_capacity(oldCapacity);
        array->values
            = grow_array(value_t, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count += 1;
}

void
free_value_array(struct value_array_t array[static 1]) {
    free_array(value_t, array->values, array->capacity);
    init_value_array(array);
}

void
print_value(value_t value) {
    printf("%g", value);
}
