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

#ifndef _TRAP_H
#define _TRAP_H

#include "arch.h"
#include "codeCache.h"
#include "stackFrame.h"


// Describes OpenJDK function being intercepted
class Trap {
  private:
    const char* _func_name;
    instruction_t* _entry;
    instruction_t _breakpoint_insn;
    instruction_t _saved_insn;

  public:
    Trap(const char* func_name) : _func_name(func_name), _entry(NULL), _breakpoint_insn(BREAKPOINT) {
    }

    bool resolve(NativeCodeCache* libjvm);
    void install();
    void uninstall();

    friend class AllocTracer;
    friend class SurvivorTracer;
};

#endif // _TRAP_H
