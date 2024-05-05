#include "chunk.h"

#include "memory.h"
#include "value.h"

void
init_chunk(struct chunk_t chunk[static 1]) {
    *chunk = (struct chunk_t){
        .count    = 0,
        .capacity = 0,
        .lines    = nullptr,
        .code     = nullptr,
    };
    init_value_array(&chunk->constants);
}

void
free_chunk(struct chunk_t chunk[static 1]) {
    free_array(u8, chunk->code, chunk->capacity);
    free_array(i32, chunk->lines, chunk->capacity);
    free_value_array(&chunk->constants);
    init_chunk(chunk);
}

void
write_chunk(struct chunk_t chunk[static 1], u8 byte, i32 line) {
    if (chunk->capacity < chunk->count + 1) {
        i32 old_capacity = chunk->capacity;
        chunk->capacity  = grow_capacity(old_capacity);
        chunk->code
            = grow_array(u8, chunk->code, old_capacity, chunk->capacity);
        chunk->lines
            = grow_array(i32, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count]  = byte;
    chunk->lines[chunk->count] = line;
    chunk->count += 1;
}

i32
add_constant(struct chunk_t chunk[static 1], value_t value) {
    write_value_array(&chunk->constants, value);
    return chunk->constants.count - 1;
}
