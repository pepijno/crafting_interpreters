#include "memory.h"

#include "compiler.h"
#include "object.h"
#include "table.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"

#include <stdio.h>
#endif

#include <stdlib.h>

#define GC_HEAP_GROW_FACTOR 2

void*
reallocate(void* pointer, i32 old_size, i32 new_size) {
    vm.bytes_allocated += new_size - old_size;
    if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
        collect_garbage();
#endif
        if (vm.bytes_allocated > vm.next_gc) {
            collect_garbage();
        }
    }

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

void
mark_object(struct object object[static 1]) {
    if (object == nullptr) {
        return;
    }
    if (object->is_marked) {
        return;
    }
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*) object);
    print_value(OBJECT_VAL(object));
    printf("\n");
#endif
    object->is_marked = true;

    if (vm.gray_capacity < vm.gray_count + 1) {
        vm.gray_capacity = grow_capacity(vm.gray_capacity);
        vm.gray_stack    = (struct object**) realloc(
            vm.gray_stack, sizeof(struct object*) * vm.gray_capacity
        );

        if (vm.gray_stack == nullptr) {
            exit(1);
        }
    }

    vm.gray_stack[vm.gray_count] = object;
    vm.gray_count += 1;
}

void
mark_value(struct value value) {
    if (IS_OBJECT(value)) {
        mark_object(AS_OBJECT(value));
    }
}

static void
mark_array(struct value_array array[static 1]) {
    for (int i = 0; i < array->count; i++) {
        mark_value(array->values[i]);
    }
}

static void
free_object(struct object object[static 1]) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*) object, object->type);
#endif
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
            free_array(
                struct object_upvalue*, closure->upvalues,
                closure->upvalue_count
            );
            FREE(struct object_closure, object);
            break;
        }
        case OBJECT_UPVALUE:
            FREE(struct object_upvalue, object);
            break;
        case OBJECT_CLASS: {
            FREE(struct object_class, object);
            break;
        }
        case OBJECT_INSTANCE: {
            struct object_instance* instance = (struct object_instance*) object;
            free_table(&instance->fields);
            FREE(struct object_instance, object);
            break;
        }
    }
}

static void
mark_roots() {
    for (struct value* slot = vm.stack; slot < vm.stack_top; slot++) {
        mark_value(*slot);
    }

    for (int i = 0; i < vm.frame_count; i++) {
        mark_object((struct object*) vm.frames[i].closure);
    }

    for (struct object_upvalue* upvalue = vm.open_upvalues; upvalue != nullptr;
         upvalue                        = upvalue->next) {
        mark_object((struct object*) upvalue);
    }

    mark_table(&vm.globals);
    mark_compiler_roots();
}

static void
blacken_object(struct object object[static 1]) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*) object);
    print_value(OBJECT_VAL(object));
    printf("\n");
#endif
    switch (object->type) {
        case OBJECT_CLOSURE: {
            struct object_closure* closure = (struct object_closure*) object;
            mark_object((struct object*) closure->function);
            for (int i = 0; i < closure->upvalue_count; i++) {
                mark_object((struct object*) closure->upvalues[i]);
            }
            break;
        }
        case OBJECT_FUNCTION: {
            struct object_function* function = (struct object_function*) object;
            mark_object((struct object*) function->name);
            mark_array(&function->chunk.constants);
            break;
        }
        case OBJECT_UPVALUE:
            mark_value(((struct object_upvalue*) object)->closed);
            break;
        case OBJECT_CLASS: {
            struct object_class* class = (struct object_class*) object;
            mark_object((struct object*) class->name);
            break;
        }
        case OBJECT_INSTANCE: {
            struct object_instance* instance = (struct object_instance*) object;
            mark_object((struct object*) instance->class);
            mark_table(&instance->fields);
            break;
        }
        case OBJECT_NATIVE:
        case OBJECT_STRING:
            break;
    }
}

static void
trace_references() {
    while (vm.gray_count > 0) {
        vm.gray_count -= 1;
        struct object* object = vm.gray_stack[vm.gray_count];
        blacken_object(object);
    }
}

static void
sweep() {
    struct object* previous = nullptr;
    struct object* object   = vm.objects;
    while (object != nullptr) {
        if (object->is_marked) {
            object->is_marked = false;
            previous          = object;
            object            = object->next;
        } else {
            struct object* unreached = object;
            object                   = object->next;
            if (previous != nullptr) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            free_object(unreached);
        }
    }
}

void
collect_garbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    uint64_t before = vm.bytes_allocated;
#endif

    mark_roots();
    trace_references();
    table_remove_white(&vm.strings);
    sweep();

    vm.next_gc = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf(
        "   collected %zu bytes (from %zu to %zu) next at %zu\n",
        before - vm.bytes_allocated, before, vm.bytes_allocated, vm.next_gc
    );
#endif
}

void
free_objects() {
    struct object* object = vm.objects;
    while (object != nullptr) {
        struct object* next = object->next;
        free_object(object);
        object = next;
    }

    free(vm.gray_stack);
}
