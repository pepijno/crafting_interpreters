#pragma once

#include "common.h"
#include "value.h"

struct entry_t {
    struct object_string_t* key;
    struct value_t value;
};

struct table_t {
    i32 count;
    i32 capacity;
    struct entry_t* entries;
};

void init_table(struct table_t table[static 1]);
void free_table(struct table_t table[static 1]);
bool table_get(
    struct table_t* table, struct object_string_t* key, struct value_t* value
);
bool table_set(
    struct table_t table[static 1], struct object_string_t* key,
    struct value_t value
);
bool table_delete(struct table_t* table, struct object_string_t* key);
void table_add_all(struct table_t* from, struct table_t* to);
struct object_string_t* table_find_string(
    struct table_t* table, char const* chars, i32 length, u32 hash
);
