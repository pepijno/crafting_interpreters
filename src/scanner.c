#include "scanner.h"

#include <string.h>

struct scanner_t {
    char const* start;
    char const* current;
    i32 line;
};

struct scanner_t scanner;

void
init_scanner(char const* source) {
    scanner.start   = source;
    scanner.current = source;
    scanner.line    = 1;
}

static bool
is_at_end() {
    return *scanner.current == '\0';
}

static struct token_t
make_token(enum token_type_e type) {
    return (struct token_t){
        .type   = type,
        .start  = scanner.start,
        .length = (i32) (scanner.current - scanner.start),
        .line   = scanner.line,
    };
}

static struct token_t
error_token(char const* message) {
    return (struct token_t){
        .type   = TOKEN_ERROR,
        .start  = message,
        .length = strlen(message),
        .line   = scanner.line,
    };
}

static char
advance() {
    scanner.current += 1;
    return scanner.current[-1];
}

static bool
match(char expected) {
    if (is_at_end()) {
        return false;
    }
    if (*scanner.current != expected) {
        return false;
    }
    scanner.current += 1;
    return true;
}

static char
peek() {
    return *scanner.current;
}

static char
peek_next() {
    if (is_at_end()) {
        return '\0';
    }
    return scanner.current[1];
}

static void
skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line += 1;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !is_at_end()) {
                        advance();
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static struct token_t
string() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            scanner.line += 1;
        }
        advance();
    }

    if (is_at_end()) {
        return error_token("Unterminated string.");
    }

    // The closing quote.
    advance();
    return make_token(TOKEN_STRING);
}

static bool
is_digit(char c) {
    return c >= '0' && c <= '9';
}

static struct token_t
number() {
    while (is_digit(peek())) {
        advance();
    }

    // Look for a fractional part.
    if (peek() == '.' && is_digit(peek_next())) {
        // Consume the ".".
        advance();

        while (is_digit(peek())) {
            advance();
        }
    }

    return make_token(TOKEN_NUMBER);
}

static bool
is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static enum token_type_e
check_keyword(i32 start, i32 length, char const* rest, enum token_type_e type) {
    if (scanner.current - scanner.start == start + length
        && memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static enum token_type_e
identifier_type() {
    switch (scanner.start[0]) {
        case 'a':
            return check_keyword(1, 2, "nd", TOKEN_AND);
        case 'c':
            return check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e':
            return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u':
                        return check_keyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            return check_keyword(1, 1, "f", TOKEN_IF);
        case 'n':
            return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o':
            return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p':
            return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r':
            return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            return check_keyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h':
                        return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v':
            return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w':
            return check_keyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

static struct token_t
identifier() {
    while (is_alpha(peek()) || is_digit(peek())) {
        advance();
    }
    return make_token(identifier_type());
}

struct token_t
scan_token() {
    skip_whitespace();
    scanner.start = scanner.current;

    if (is_at_end()) {
        return make_token(TOKEN_EOF);
    }

    char c = advance();
    if (is_alpha(c)) {
        return identifier();
    }
    if (is_digit(c)) {
        return number();
    }

    switch (c) {
        case '(':
            return make_token(TOKEN_LEFT_PAREN);
        case ')':
            return make_token(TOKEN_RIGHT_PAREN);
        case '{':
            return make_token(TOKEN_LEFT_BRACE);
        case '}':
            return make_token(TOKEN_RIGHT_BRACE);
        case ';':
            return make_token(TOKEN_SEMICOLON);
        case ',':
            return make_token(TOKEN_COMMA);
        case '.':
            return make_token(TOKEN_DOT);
        case '-':
            return make_token(TOKEN_MINUS);
        case '+':
            return make_token(TOKEN_PLUS);
        case '/':
            return make_token(TOKEN_SLASH);
        case '*':
            return make_token(TOKEN_STAR);
        case '!':
            return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            return string();
    }

    return error_token("Unexpected character.");
}
