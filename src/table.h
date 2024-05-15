#pragma once

#include "common.h"
#include "value.h"

struct entry {
    struct object_string* key;
    struct value value;
};

struct table {
    i32 count;
    i32 capacity;
    struct entry* entries;
};

void init_table(struct table table[static 1]);
void free_table(struct table table[static 1]);
bool table_get(
    struct table* table, struct object_string* key, struct value* value
);
bool table_set(
    struct table table[static 1], struct object_string* key, struct value value
);
bool table_delete(struct table* table, struct object_string* key);
void table_add_all(struct table* from, struct table* to);
struct object_string* table_find_string(
    struct table* table, char const* chars, i32 length, u32 hash
);
void table_remove_white(struct table* table);
void mark_table(struct table* table);
