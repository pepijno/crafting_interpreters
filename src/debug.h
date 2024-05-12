#pragma once

#include "chunk.h"

void disassemble_chunk(struct chunk chunk[static 1], char const* name);
i32 disassemble_instruction(struct chunk chunk[static 1], i32 offset);
