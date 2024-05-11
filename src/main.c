#include "vm.h"

#include <stdio.h>
#include <stdlib.h>

static char*
read_file(char const* path) {
    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*) malloc(fileSize + 1);
    if (buffer == nullptr) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void
run_file(char const* path) {
    char* source                   = read_file(path);
    enum interpret_result_e result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) {
        exit(65);
    }
    if (result == INTERPRET_RUNTIME_ERROR) {
        exit(70);
    }
}

static void
repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

int
main(int argc, char const* argv[]) {
    init_vm();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }

    // struct chunk_t chunk;
    // init_chunk(&chunk);
    //
    // i32 constant = add_constant(&chunk, 1.2);
    // write_chunk(&chunk, OP_CONSTANT, 123);
    // write_chunk(&chunk, constant, 123);
    // write_chunk(&chunk, OP_NEGATE, 123);
    //
    // constant = add_constant(&chunk, 3.4);
    // write_chunk(&chunk, OP_CONSTANT, 123);
    // write_chunk(&chunk, constant, 123);
    //
    // write_chunk(&chunk, OP_ADD, 123);
    //
    // constant = add_constant(&chunk, 5.6);
    // write_chunk(&chunk, OP_CONSTANT, 123);
    // write_chunk(&chunk, constant, 123);
    //
    // write_chunk(&chunk, OP_DIVIDE, 123);
    //
    // write_chunk(&chunk, OP_RETURN, 123);
    //
    // // disassemble_chunk(&chunk, "test chunk");
    //
    // interpret(&chunk);
    //
    // free_chunk(&chunk);
    return 0;
}
