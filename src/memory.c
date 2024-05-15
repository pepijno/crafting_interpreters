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
free_object(struct object object[static 1]) {
    switch (object->type) {
        case OBJECT_STRING: {
            struct object_string* string = (struct object_string*) object;
            free_array(char, string->chars, string->length + 1);
            FREE(struct object_string, object);
            break;
        }
        case OBJECT_FUNCTION: {
            struct object_function* function = (struct object_function*) object;
            free_chunk(&function->chunk);
            FREE(struct object_function, object);
            break;
        }
        case OBJECT_NATIVE: {
            FREE(struct object_native, object);
            break;
        }
        case OBJECT_CLOSURE: {
            struct object_closure* closure = (struct object_closure*) object;
            free_array(struct object_upvalue*, closure->upvalues, closure->upvalue_count);
            FREE(struct object_closure, object);
            break;
        }
        case OBJECT_UPVALUE:
            FREE(struct object_upvalue, object);
            break;
    }
}

void
free_objects() {
    struct object* object = vm.objects;
    while (object != nullptr) {
        struct object* next = object->next;
        free_object(object);
        object = next;
    }
}
