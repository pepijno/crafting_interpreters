#include "memory.h"

#include <stdlib.h>

void* reallocate(void* pointer, i32 old_size, i32 new_size) {
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
