#include "table.h"

#include "memory.h"
#include "object.h"
#include "value.h"

#include <string.h>

#define TABLE_MAX_LOAD 0.75

void
init_table(struct table_t table[static 1]) {
    table->count    = 0;
    table->capacity = 0;
    table->entries  = nullptr;
}

void
free_table(struct table_t table[static 1]) {
    free_array(struct entry_t, table->entries, table->capacity);
    init_table(table);
}

static struct entry_t*
find_entry(struct entry_t* entries, i32 capacity, struct object_string_t* key) {
    uint32_t index            = key->hash % capacity;
    struct entry_t* tombstone = nullptr;
    for (;;) {
        struct entry_t* entry = &entries[index];
        if (entry->key == nullptr) {
            if (IS_NIL(entry->value)) {
                // Empty entry.
                return tombstone != nullptr ? tombstone : entry;
            } else {
                // We found a tombstone.
                if (tombstone == nullptr) {
                    tombstone = entry;
                }
            }
        } else if (entry->key == key) {
            // We found the key.
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool
table_get(
    struct table_t* table, struct object_string_t* key, struct value_t* value
) {
    if (table->count == 0) {
        return false;
    }

    struct entry_t* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == nullptr) {
        return false;
    }

    *value = entry->value;
    return true;
}

static void
adjust_capacity(struct table_t* table, int capacity) {
    struct entry_t* entries = ALLOCATE(struct entry_t, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key   = nullptr;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        struct entry_t* entry = &table->entries[i];
        if (entry->key == nullptr) {
            continue;
        }

        struct entry_t* dest = find_entry(entries, capacity, entry->key);
        dest->key            = entry->key;
        dest->value          = entry->value;
        table->count += 1;
    }

    free_array(struct entry_t, table->entries, table->capacity);
    table->entries  = entries;
    table->capacity = capacity;
}

bool
table_set(
    struct table_t table[static 1], struct object_string_t* key,
    struct value_t value
) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = grow_capacity(table->capacity);
        adjust_capacity(table, capacity);
    }

    struct entry_t* entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key       = entry->key == nullptr;
    if (is_new_key && IS_NIL(entry->value)) {
        table->count += 1;
    }

    entry->key   = key;
    entry->value = value;
    return is_new_key;
}

bool
table_delete(struct table_t* table, struct object_string_t* key) {
    if (table->count == 0) {
        return false;
    }

    // Find the entry.
    struct entry_t* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == nullptr) {
        return false;
    }

    // Place a tombstone in the entry.
    entry->key   = nullptr;
    entry->value = BOOL_VAL(true);
    return true;
}

void
tableAddAll(struct table_t* from, struct table_t* to) {
    for (int i = 0; i < from->capacity; i++) {
        struct entry_t* entry = &from->entries[i];
        if (entry->key != nullptr) {
            table_set(to, entry->key, entry->value);
        }
    }
}

struct object_string_t*
table_find_string(
    struct table_t* table, char const* chars, i32 length, u32 hash
) {
    if (table->count == 0) {
        return nullptr;
    }

    u32 index = hash % table->capacity;
    for (;;) {
        struct entry_t* entry = &table->entries[index];
        if (entry->key == nullptr) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) {
                return nullptr;
            }
        } else if (entry->key->length == length &&
        entry->key->hash == hash &&
        memcmp(entry->key->chars, chars, length) == 0) {
            // We found it.
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}
