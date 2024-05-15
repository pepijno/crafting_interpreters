#include "compiler.h"

#include "memory.h"

#include <string.h>

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif
#include "object.h"
#include "scanner.h"

#include <stdio.h>
#include <stdlib.h>

struct parser {
    struct token current;
    struct token previous;
    bool had_error;
    bool panic_mode;
};

enum precedence {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
};

enum function_type {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
    TYPE_METHOD,
    TYPE_INITIALIZER,
};

typedef void (*parse_function)(bool can_assign);

struct parse_rule {
    parse_function prefix;
    parse_function infix;
    enum precedence precedence;
};

struct local {
    struct token name;
    i32 depth;
    bool is_captured;
};

struct upvalue {
    u8 index;
    bool is_local;
};

struct compiler {
    struct compiler* enclosing;
    struct object_function* function;
    enum function_type type;

    struct local locals[UINT8_COUNT];
    i32 local_count;
    struct upvalue upvalues[UINT8_COUNT];
    i32 scope_depth;
};

struct class_compiler {
    struct class_compiler* enclosing;
};

struct parser parser;
struct compiler* current             = nullptr;
struct class_compiler* current_class = nullptr;

static struct chunk*
current_chunk() {
    return &current->function->chunk;
}

static void
error_at(struct token token[static 1], char const* message) {
    if (parser.panic_mode) {
        return;
    }
    parser.panic_mode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void
error_at_current(char const* message) {
    error_at(&parser.current, message);
}

static void
error(char const* message) {
    error_at(&parser.previous, message);
}

static void
advance() {
    parser.previous = parser.current;

    while (true) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) {
            break;
        }

        error_at_current(parser.current.start);
    }
}

static void
consume(enum token_type type, char const* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    error_at_current(message);
}

static bool
check(enum token_type type) {
    return parser.current.type == type;
}

static bool
match(enum token_type type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

static void
emit_byte(u8 byte) {
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void
emit_bytes(u8 byte1, u8 byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void
emit_loop(i32 loop_start) {
    emit_byte(OP_LOOP);

    i32 offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }

    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static i32
emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->count - 2;
}

static void
emit_return() {
    if (current->type == TYPE_INITIALIZER) {
        emit_bytes(OP_GET_LOCAL, 0);
    } else {
        emit_byte(OP_NIL);
    }

    emit_byte(OP_RETURN);
}

static u8
make_constant(struct value value) {
    i32 constant = add_constant(current_chunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (u8) constant;
}

static void
emit_constant(struct value value) {
    emit_bytes(OP_CONSTANT, make_constant(value));
}

static void
patch_jump(i32 offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    i32 jump = current_chunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    current_chunk()->code[offset]     = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

static void
init_compiler(struct compiler compiler[static 1], enum function_type type) {
    compiler->enclosing   = current;
    compiler->function    = nullptr;
    compiler->type        = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function    = new_function();
    current               = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name
            = copy_string(parser.previous.start, parser.previous.length);
    }

    struct local* local = &current->locals[current->local_count];
    current->local_count += 1;
    local->is_captured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start  = "this";
        local->name.length = 4;
    } else {
        local->name.start  = "";
        local->name.length = 0;
    }
}

static struct object_function*
end_compiler() {
    emit_return();
    struct object_function* function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        disassemble_chunk(
            current_chunk(),
            function->name != nullptr ? function->name->chars : "<script>"
        );
    }
#endif

    current = current->enclosing;
    return function;
}

static void
begin_scope() {
    current->scope_depth += 1;
}

static void
end_scope() {
    current->scope_depth -= 1;
    while (current->local_count > 0
           && current->locals[current->local_count - 1].depth
                  > current->scope_depth) {
        if (current->locals[current->local_count - 1].is_captured) {
            emit_byte(OP_CLOSE_UPVALUE);
        } else {
            emit_byte(OP_POP);
        }
        current->local_count--;
    }
}

static void expression();
static void statement();
static void declaration();
static struct parse_rule* get_rule(enum token_type type);
static void parse_precedence(enum precedence precedence);

static void
and_(bool can_assign) {
    (void) can_assign;
    i32 end_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}

static void
or_(bool can_assign) {
    (void) can_assign;
    i32 else_jump = emit_jump(OP_JUMP_IF_FALSE);
    i32 end_jump  = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);

    parse_precedence(PREC_OR);
    patch_jump(end_jump);
}

static void
binary(bool can_assign) {
    (void) can_assign;
    enum token_type operatorType = parser.previous.type;
    struct parse_rule* rule      = get_rule(operatorType);
    parse_precedence((enum precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:
            emit_bytes(OP_EQUAL, OP_NOT);
            break;
        case TOKEN_EQUAL_EQUAL:
            emit_byte(OP_EQUAL);
            break;
        case TOKEN_GREATER:
            emit_byte(OP_GREATER);
            break;
        case TOKEN_GREATER_EQUAL:
            emit_bytes(OP_LESS, OP_NOT);
            break;
        case TOKEN_LESS:
            emit_byte(OP_LESS);
            break;
        case TOKEN_LESS_EQUAL:
            emit_bytes(OP_GREATER, OP_NOT);
            break;
        case TOKEN_PLUS:
            emit_byte(OP_ADD);
            break;
        case TOKEN_MINUS:
            emit_byte(OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emit_byte(OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emit_byte(OP_DIVIDE);
            break;
        default:
            return; // Unreachable.
    }
}

static u8
argument_list() {
    u8 arg_count = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (arg_count == 255) {
                error("Can't have more than 255 arguments.");
            }
            arg_count += 1;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return arg_count;
}

static u8
identifier_constant(struct token name[static 1]) {
    return make_constant(OBJECT_VAL(copy_string(name->start, name->length)));
}

static void
call(bool can_assign) {
    (void) can_assign;
    u8 argCount = argument_list();
    emit_bytes(OP_CALL, argCount);
}

static void
dot(bool can_assign) {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    u8 name = identifier_constant(&parser.previous);

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
        uint8_t arg_count = argument_list();
        emit_bytes(OP_INVOKE, name);
        emit_byte(arg_count);
    } else {
        emit_bytes(OP_GET_PROPERTY, name);
    }
}

static void
literal(bool can_assign) {
    (void) can_assign;
    switch (parser.previous.type) {
        case TOKEN_FALSE:
            emit_byte(OP_FALSE);
            break;
        case TOKEN_NIL:
            emit_byte(OP_NIL);
            break;
        case TOKEN_TRUE:
            emit_byte(OP_TRUE);
            break;
        default:
            return; // Unreachable.
    }
}

static void
grouping(bool can_assign) {
    (void) can_assign;
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void
number(bool can_assign) {
    (void) can_assign;
    double value = strtod(parser.previous.start, nullptr);
    emit_constant(NUMBER_VAL(value));
}

static void
string(bool can_assign) {
    (void) can_assign;
    emit_constant(OBJECT_VAL(
        copy_string(parser.previous.start + 1, parser.previous.length - 2)
    ));
}

static bool
identifiers_equal(struct token a[static 1], struct token b[static 1]) {
    if (a->length != b->length) {
        return false;
    }
    return memcmp(a->start, b->start, a->length) == 0;
}

static i32
resolve_local(struct compiler compiler[static 1], struct token name[static 1]) {
    for (i32 i = compiler->local_count - 1; i >= 0; i--) {
        struct local* local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static i32
add_upvalue(struct compiler* compiler, u8 index, bool is_local) {
    i32 upvalue_count = compiler->function->upvalue_count;

    for (i32 i = 0; i < upvalue_count; i++) {
        struct upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (upvalue_count == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index    = index;
    return compiler->function->upvalue_count++;
}

static i32
resolve_upvalue(
    struct compiler compiler[static 1], struct token name[static 1]
) {
    if (compiler->enclosing == nullptr) {
        return -1;
    }

    i32 local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (u8) local, true);
    }

    i32 upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (u8) upvalue, false);
    }

    return -1;
}

static void
add_local(struct token name) {
    if (current->local_count == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    struct local* local = &current->locals[current->local_count];
    current->local_count += 1;
    local->name        = name;
    local->depth       = -1;
    local->is_captured = false;
}

static void
declare_variable() {
    if (current->scope_depth == 0) {
        return;
    }
    struct token* name = &parser.previous;
    for (i32 i = current->local_count - 1; i >= 0; i--) {
        struct local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    add_local(*name);
}

static void
named_variable(struct token name, bool can_assign) {
    uint8_t get_op, set_op;
    i32 arg = resolve_local(current, &name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else if ((arg = resolve_upvalue(current, &name)) != -1) {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    } else {
        arg    = identifier_constant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }
    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(set_op, (uint8_t) arg);
    } else {
        emit_bytes(get_op, (uint8_t) arg);
    }
}

static void
variable(bool can_assign) {
    named_variable(parser.previous, can_assign);
}

static void this(bool can_assign) {
    (void) can_assign;
    if (current_class == nullptr) {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(false);
}

static void
unary(bool can_assign) {
    (void) can_assign;
    enum token_type operatorType = parser.previous.type;

    // Compile the operand.
    parse_precedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_BANG:
            emit_byte(OP_NOT);
            break;
        case TOKEN_MINUS:
            emit_byte(OP_NEGATE);
            break;
        default:
            return; // Unreachable.
    }
}

struct parse_rule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping,    call,       PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_LEFT_BRACE]    = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_RIGHT_BRACE]   = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_COMMA]         = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_DOT]           = { nullptr,     dot,       PREC_CALL},
    [TOKEN_MINUS]         = {   unary,  binary,       PREC_TERM},
    [TOKEN_PLUS]          = { nullptr,  binary,       PREC_TERM},
    [TOKEN_SEMICOLON]     = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_SLASH]         = { nullptr,  binary,     PREC_FACTOR},
    [TOKEN_STAR]          = { nullptr,  binary,     PREC_FACTOR},
    [TOKEN_BANG]          = {   unary, nullptr,       PREC_NONE},
    [TOKEN_BANG_EQUAL]    = { nullptr,  binary,   PREC_EQUALITY},
    [TOKEN_EQUAL]         = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = { nullptr,  binary,   PREC_EQUALITY},
    [TOKEN_GREATER]       = { nullptr,  binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = { nullptr,  binary, PREC_COMPARISON},
    [TOKEN_LESS]          = { nullptr,  binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = { nullptr,  binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, nullptr,       PREC_NONE},
    [TOKEN_STRING]        = {  string, nullptr,       PREC_NONE},
    [TOKEN_NUMBER]        = {  number, nullptr,       PREC_NONE},
    [TOKEN_AND]           = { nullptr,    and_,        PREC_AND},
    [TOKEN_CLASS]         = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_ELSE]          = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_FALSE]         = { literal, nullptr,       PREC_NONE},
    [TOKEN_FOR]           = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_FUN]           = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_IF]            = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_NIL]           = { literal, nullptr,       PREC_NONE},
    [TOKEN_OR]            = { nullptr,     or_,         PREC_OR},
    [TOKEN_PRINT]         = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_RETURN]        = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_SUPER]         = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_THIS]          = {    this, nullptr,       PREC_NONE},
    [TOKEN_TRUE]          = { literal, nullptr,       PREC_NONE},
    [TOKEN_VAR]           = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_WHILE]         = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_ERROR]         = { nullptr, nullptr,       PREC_NONE},
    [TOKEN_EOF]           = { nullptr, nullptr,       PREC_NONE},
};

static void
parse_precedence(enum precedence precedence) {
    advance();
    parse_function prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == nullptr) {
        error("Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        parse_function infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static u8
parse_variable(char const* error_message) {
    consume(TOKEN_IDENTIFIER, error_message);

    declare_variable();
    if (current->scope_depth > 0) {
        return 0;
    }

    return identifier_constant(&parser.previous);
}

static void
mark_initialized() {
    if (current->scope_depth == 0) {
        return;
    }
    current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void
define_variable(u8 global) {
    if (current->scope_depth > 0) {
        mark_initialized();
        return;
    }
    emit_bytes(OP_DEFINE_GLOBAL, global);
}

static struct parse_rule*
get_rule(enum token_type type) {
    return &rules[type];
}

static void
expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void
block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void
function(enum function_type type) {
    struct compiler compiler;
    init_compiler(&compiler, type);
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity += 1;
            if (current->function->arity > 255) {
                error_at_current("Can't have more than 255 parameters.");
            }
            u8 constant = parse_variable("Expected parameter name.");
            define_variable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    struct object_function* function = end_compiler();
    emit_bytes(OP_CLOSURE, make_constant(OBJECT_VAL(function)));

    for (i32 i = 0; i < function->upvalue_count; i++) {
        emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(compiler.upvalues[i].index);
    }
}

static void
method() {
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant        = identifier_constant(&parser.previous);
    enum function_type type = TYPE_METHOD;
    if (parser.previous.length == 4
        && memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(type);
    emit_bytes(OP_METHOD, constant);
}

static void
class_declaration() {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    struct token class_name = parser.previous;
    uint8_t nameConstant    = identifier_constant(&parser.previous);
    declare_variable();

    emit_bytes(OP_CLASS, nameConstant);
    define_variable(nameConstant);

    struct class_compiler class_compiler;
    class_compiler.enclosing = current_class;
    current_class            = &class_compiler;

    named_variable(class_name, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emit_byte(OP_POP);

    current_class = current_class->enclosing;
}

static void
fun_declaration() {
    uint8_t global = parse_variable("Expect function name.");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
}

static void
var_declaration() {
    u8 global = parse_variable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    define_variable(global);
}

static void
expression_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression");
    emit_byte(OP_POP);
}

static void
if_statement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();

    i32 else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);
    emit_byte(OP_POP);

    if (match(TOKEN_ELSE)) {
        statement();
    }

    patch_jump(else_jump);
}

static void
print_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value");
    emit_byte(OP_PRINT);
}

static void
return_statement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }
    if (match(TOKEN_SEMICOLON)) {
        emit_return();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN);
    }
}

static void
while_statement() {
    i32 loop_start = current_chunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    i32 exitJump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);

    patch_jump(exitJump);
    emit_byte(OP_POP);
}

static void
for_statement() {
    begin_scope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        expression_statement();
    }

    i32 loop_start = current_chunk()->count;
    i32 exit_jump  = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP); // Condition.
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        i32 body_jump       = emit_jump(OP_JUMP);
        i32 increment_start = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }

    statement();
    emit_loop(loop_start);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP); // Condition.
    }
    end_scope();
}

static void
synchronize() {
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) {
            return;
        }
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:; // Do nothing.
        }

        advance();
    }
}

static void
declaration() {
    if (match(TOKEN_CLASS)) {
        class_declaration();
    } else if (match(TOKEN_FUN)) {
        fun_declaration();
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        statement();
    }

    if (parser.panic_mode) {
        synchronize();
    }
}

static void
statement() {
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else if (match(TOKEN_IF)) {
        if_statement();
    } else if (match(TOKEN_RETURN)) {
        return_statement();
    } else if (match(TOKEN_WHILE)) {
        while_statement();
    } else if (match(TOKEN_FOR)) {
        for_statement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else {
        expression_statement();
    }
}

struct object_function*
compile(char const* source) {
    init_scanner(source);
    struct compiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    parser.had_error  = false;
    parser.panic_mode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    struct object_function* function = end_compiler();
    return parser.had_error ? nullptr : function;
}

void
mark_compiler_roots() {
    struct compiler* compiler = current;
    while (compiler != nullptr) {
        mark_object((struct object*) compiler->function);
        compiler = compiler->enclosing;
    }
}
