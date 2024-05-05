#include "debug.h"

#include <stdio.h>

void
disassemble_chunk(struct chunk_t chunk[static 1], char const* name) {
    printf("=== %s ===\n", name);

    for (i32 offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}

static int
simple_instruction(char const* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int
constant_instruction(
    char const* name, struct chunk_t chunk[static 1], i32 offset
) {
    u8 constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

i32
disassemble_instruction(struct chunk_t chunk[static 1], i32 offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    u8 instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode: %d\n", instruction);
            return offset + 1;
    }
}
