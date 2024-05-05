#pragma once

#include "chunk.h"

void disassemble_chunk(struct chunk_t chunk[static 1], char const* name);
i32 disassemble_instruction(struct chunk_t chunk[static 1], i32 offset);
