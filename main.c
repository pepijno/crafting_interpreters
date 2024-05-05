#include "chunk.h"
#include "common.h"
#include "debug.h"

int
main(int argc, char const* argv[]) {
    struct chunk_t chunk;
    init_chunk(&chunk);

	i32 constant = add_constant(&chunk, 1.2);
	write_chunk(&chunk, OP_CONSTANT, 123);
	write_chunk(&chunk, constant, 123);

    write_chunk(&chunk, OP_RETURN, 123);

    disassemble_chunk(&chunk, "test chunk");
    free_chunk(&chunk);
    return 0;
}