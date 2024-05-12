#include "vm.h"

#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct vm vm;

static struct value clock_native(i32 arg_count, struct value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void
reset_stack() {
    vm.stack_top   = vm.stack;
    vm.frame_count = 0;
}

static void
runtime_error(char const* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (i32 i = vm.frame_count - 1; i >= 0; i--) {
        struct call_frame* frame         = &vm.frames[i];
        struct object_function* function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    reset_stack();
}

static void
define_native(char const* name, native_function function) {
    push(OBJECT_VAL(copy_string(name, (int) strlen(name))));
    push(OBJECT_VAL(new_native(function)));
    table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void
init_vm() {
    reset_stack();
    vm.objects = nullptr;
    init_table(&vm.globals);
    init_table(&vm.strings);

    define_native("clock", clock_native);
}

void
free_vm() {
    free_table(&vm.strings);
    free_table(&vm.globals);
    free_objects();
}

void
push(struct value value) {
    *vm.stack_top = value;
    vm.stack_top += 1;
}

struct value
pop() {
    vm.stack_top -= 1;
    return *vm.stack_top;
}

static struct value
peek(i32 distance) {
    return vm.stack_top[-1 - distance];
}

static bool
call(struct object_function* function, i32 arg_count) {
    if (arg_count != function->arity) {
        runtime_error(
            "Expected %d arguments but got %d.", function->arity, arg_count
        );
        return false;
    }

    if (vm.frame_count == FRAMES_MAX) {
        runtime_error("Stack overflow.");
        return false;
    }

    struct call_frame* frame = &vm.frames[vm.frame_count];
    vm.frame_count += 1;
    frame->function = function;
    frame->ip       = function->chunk.code;
    frame->slots    = vm.stack_top - arg_count - 1;
    return true;
}

static bool
call_value(struct value callee, i32 arg_count) {
    if (IS_OBJECT(callee)) {
        switch (OBJECT_TYPE(callee)) {
            case OBJECT_FUNCTION:
                return call(AS_FUNCTION(callee), arg_count);
            case OBJECT_NATIVE: {
                native_function native = AS_NATIVE(callee);
                struct value result
                    = native(arg_count, vm.stack_top - arg_count);
                vm.stack_top -= arg_count - 1;
                push(result);
                return true;
            }
            default:
                break; // Non-callable object type.
        }
    }
    runtime_error("Can only call functions and classes.");
    return false;
}

static bool
is_falsey(struct value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void
concatenate() {
    struct object_string* b = AS_STRING(pop());
    struct object_string* a = AS_STRING(pop());

    i32 length  = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    struct object_string* result = take_string(chars, length);
    push(OBJECT_VAL(result));
}

static enum interpret_result
run() {
    struct call_frame* frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t) ((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op)                          \
    do {                                                  \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers.");   \
            return INTERPRET_RUNTIME_ERROR;               \
        }                                                 \
        double b = AS_NUMBER(pop());                      \
        double a = AS_NUMBER(pop());                      \
        push(valueType(a op b));                          \
    } while (false)

    while (true) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (struct value* slot = vm.stack; slot < vm.stack_top; slot += 1) {
            printf("[");
            print_value(*slot);
            printf("]");
        }
        printf("\n");
        disassemble_instruction(
            &frame->function->chunk,
            (i32) (frame->ip - frame->function->chunk.code)
        );
#endif
        u8 instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                struct value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL:
                push(NIL_VAL);
                break;
            case OP_TRUE:
                push(BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(BOOL_VAL(false));
                break;
            case OP_POP:
                pop();
                break;
            case OP_DEFINE_GLOBAL: {
                struct object_string* name = READ_STRING();
                table_set(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_GET_GLOBAL: {
                struct object_string* name = READ_STRING();
                struct value value;
                if (!table_get(&vm.globals, name, &value)) {
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot       = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_SET_GLOBAL: {
                struct object_string* name = READ_STRING();
                if (table_set(&vm.globals, name, peek(0))) {
                    table_delete(&vm.globals, name);
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                struct value b = pop();
                struct value a = pop();
                push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtime_error("Operands must be two numbers or two strings."
                    );
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;
            case OP_NOT:
                push(BOOL_VAL(is_falsey(pop())));
                break;
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }
            case OP_PRINT:
                print_value(pop());
                printf("\n");
                break;
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->function += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_falsey(peek(0))) {
                    frame->ip += offset;
                }
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                u8 arg_count = READ_BYTE();
                if (!call_value(peek(arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_RETURN: {
                struct value result = pop();
                vm.frame_count -= 1;
                if (vm.frame_count == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stack_top = frame->slots;
                push(result);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
        }
    }

#undef BINARY_OP
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef READ_BYTE
}

enum interpret_result
interpret(char const* source) {
    struct object_function* function = compile(source);
    if (function == nullptr) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(OBJECT_VAL(function));
    call(function, 0);

    return run();
}
