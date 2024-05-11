#pragma once

#include "common.h"

#define ALLOCATE(type, count) \
    (type*) reallocate(nullptr, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define grow_capacity(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)
#define grow_array(type, pointer, old_count, new_count)                 \
    (type*) reallocate(                                                 \
        (pointer), sizeof(type) * (old_count), sizeof(type) * new_count \
    )
#define free_array(type, pointer, old_count) \
    reallocate((pointer), sizeof(type) * (old_count), 0)

void* reallocate(void* pointer, i32 old_size, i32 new_size);
void free_objects();