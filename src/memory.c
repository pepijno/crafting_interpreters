#include "memory.h"

#include "object.h"
#include "vm.h"

#include <stdlib.h>

void*
reallocate(void* pointer, i32 old_size, i32 new_size) {
    if (new_size == 0) {
        free(pointer);
        return nullptr;
    }

    void* result = realloc(pointer, new_size);
    if (result == nullptr) {
        exit(1);
    }
    return result;
}

static void
free_object(struct object_t object[static 1]) {
    switch (object->type) {
        case OBJECT_STRING: {
            struct object_string_t* string = (struct object_string_t*) object;
            free_array(char, string->chars, string->length + 1);
            FREE(struct object_string_t, object);
            break;
        }
    }
}

void
free_objects() {
    struct object_t* object = vm.objects;
    while (object != nullptr) {
        struct object_t* next = object->next;
        free_object(object);
        object = next;
    }
}
