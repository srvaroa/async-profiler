/*
 * Copyright 2017 Andrei Pangin
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

#include <unistd.h>
#include <sys/mman.h>
#include "trap.h"


//
// Resolve the address of the intercepted function
bool Trap::resolve(NativeCodeCache* libjvm) {
    if (_entry != NULL) {
        return true;
    }

    uintptr_t addr = (uintptr_t)libjvm->findSymbolByPrefix(_func_name);
    if (addr == 0) {
        return false;
    }

#if defined(__arm__) || defined(__thumb__)
    if (addr & 1) {
        addr ^= 1;
        _breakpoint_insn = BREAKPOINT_THUMB;
    }
#endif

    // Make the entry point writable, so we can rewrite instructions
    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page_start = addr & -page_size;
    mprotect((void*)page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);

    _entry = (instruction_t*)addr;
    return true;
}

// Insert breakpoint at the very first instruction
void Trap::install() {
    if (_entry != NULL) {
        _saved_insn = *_entry;
        *_entry = _breakpoint_insn;
        flushCache(_entry);
    }
}

// Clear breakpoint - restore the original instruction
void Trap::uninstall() {
    if (_entry != NULL) {
        *_entry = _saved_insn;
        flushCache(_entry);
    }
}


