/*
 * Copyright 2020 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>
#include "dictionary.h"
#include "arch.h"


static inline char* allocateKey(const char* key, size_t length) {
    char* result = (char*)malloc(length + 1);
    memcpy(result, key, length);
    result[length] = 0;
    return result;
}

static inline bool keyEquals(const char* candidate, const char* key, size_t length) {
    return strncmp(candidate, key, length) == 0 && candidate[length] == 0;
}


Dictionary::Dictionary() {
    _table = (DictTable*)calloc(1, sizeof(DictTable));
    _size = 0;
}

Dictionary::~Dictionary() {
    clear(_table);
    free(_table);
}

void Dictionary::clear() {
    clear(_table);
    memset(_table, 0, sizeof(DictTable));
    _size = 0;
}

void Dictionary::clear(DictTable* table) {
    for (int i = 0; i < ROWS; i++) {
        DictRow* row = &table->rows[i];
        for (int j = 0; j < CELLS; j++) {
            free(row->keys[j]);
        }
        if (row->next != NULL) {
            clear(row->next);
            free(row->next);
        }
    }
}

// Many popular symbols are quite short, e.g. "[B", "()V" etc.
// FNV-1a is reasonably fast and sufficiently random.
unsigned int Dictionary::hash(const char* key, size_t length) {
    unsigned int h = 2166136261;
    for (size_t i = 0; i < length; i++) {
        h = (h ^ key[i]) * 16777619;
    }
    return h;
}

const char* Dictionary::lookup(const char* key) {
    return lookup(key, strlen(key));
}

const char* Dictionary::lookup(const char* key, size_t length) {
    DictTable* table = _table;
    unsigned int h = hash(key, length);

    while (true) {
        DictRow* row = &table->rows[h % ROWS];
        for (int c = 0; c < CELLS; c++) {
            if (row->keys[c] == NULL) {
                char* new_key = allocateKey(key, length);
                if (__sync_bool_compare_and_swap(&row->keys[c], NULL, new_key)) {
                    atomicInc(_size);
                    return new_key;
                }
                free(new_key);
            }
            if (keyEquals(row->keys[c], key, length)) {
                return row->keys[c];
            }
        }

        if (row->next == NULL) {
            DictTable* new_table = (DictTable*)calloc(1, sizeof(DictTable));
            if (!__sync_bool_compare_and_swap(&row->next, NULL, new_table)) {
                free(new_table);
            }
        }

        table = row->next;
        h = (h >> ROW_BITS) | (h << (32 - ROW_BITS));
    }
}

void Dictionary::collect(std::vector<const char*>& v) {
    collect(v, _table);
}

void Dictionary::collect(std::vector<const char*>& v, DictTable* table) {
    for (int i = 0; i < ROWS; i++) {
        DictRow* row = &table->rows[i];
        for (int j = 0; j < CELLS; j++) {
            if (row->keys[j] != NULL) {
                v.push_back(row->keys[j]);
            }
        }
        if (row->next != NULL) {
            collect(v, row->next);
        }
    }
}
