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

#ifndef _DICTIONARY_H
#define _DICTIONARY_H

#include <vector>
#include <stddef.h>


#define ROW_BITS  7
#define ROWS      (1 << ROW_BITS)
#define CELLS     3


struct DictTable;

struct DictRow {
    char* keys[CELLS];
    DictTable* next;
};

struct DictTable {
    DictRow rows[ROWS];
};

// Append-only concurrent hash table based on multi-level arrays
class Dictionary {
  private:
    DictTable* _table;
    volatile int _size;

    static void clear(DictTable* table);

    static unsigned int hash(const char* key, size_t length);

    static void collect(std::vector<const char*>& v, DictTable* table);

  public:
    Dictionary();
    ~Dictionary();

    int size() {
        return _size;
    }

    void clear();

    const char* lookup(const char* key);
    const char* lookup(const char* key, size_t length);

    void collect(std::vector<const char*>& v);
};

#endif // _DICTIONARY_H
