#include "object.h"

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJECT(type, objectType) \
    (type*) allocate_object(sizeof(type), objectType)

static struct object*
allocate_object(size_t size, enum object_type type) {
    struct object* object = (struct object*) reallocate(nullptr, 0, size);
    object->type            = type;
    object->next            = vm.objects;
    vm.objects              = object;
    return object;
}

struct object_function*
new_function() {
    struct object_function* function
        = ALLOCATE_OBJECT(struct object_function, OBJECT_FUNCTION);
    function->arity = 0;
    function->name  = NULL;
    init_chunk(&function->chunk);
    return function;
}

struct object_native*
new_native(native_function function) {
    struct object_native* native
        = ALLOCATE_OBJECT(struct object_native, OBJECT_NATIVE);
    native->function = function;
    return native;
}

static struct object_string*
allocate_string(char* chars, i32 length, i32 hash) {
    struct object_string* string
        = ALLOCATE_OBJECT(struct object_string, OBJECT_STRING);
    string->length = length;
    string->chars  = chars;
    string->hash   = hash;
    table_set(&vm.strings, string, NIL_VAL);
    return string;
}

static u32
hash_string(char const* key, i32 length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t) key[i];
        hash *= 16777619;
    }
    return hash;
}

struct object_string*
take_string(char* chars, i32 length) {
    u32 hash = hash_string(chars, length);
    struct object_string* interned
        = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        free_array(char, chars, length + 1);
        return interned;
    }

    return allocate_string(chars, length, hash);
}

struct object_string*
copy_string(char const* chars, i32 length) {
    u32 hash = hash_string(chars, length);
    struct object_string* interned
        = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        return interned;
    }

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocate_string(heapChars, length, hash);
}

static void
print_function(struct object_function* function) {
    if (function->name == nullptr) {
        printf("<script>");
    } else {
        printf("<fn %s>", function->name->chars);
    }
}

void
print_object(struct value value) {
    switch (OBJECT_TYPE(value)) {
        case OBJECT_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJECT_FUNCTION:
            print_function(AS_FUNCTION(value));
            break;
        case OBJECT_NATIVE:
            printf("<native fn>");
            break;
    }
}
