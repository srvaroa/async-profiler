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

#ifndef _SURVIVORTRACER_H
#define _SURVIVORTRACER_H

#include <signal.h>
#include <stdint.h>
#include "arch.h"
#include "codeCache.h"
#include "engine.h"
#include "stackFrame.h"
#include "trap.h"


class SurvivorTracer : public Engine {
  private:
    static Trap _copy_to_survivor;

    static u64 _interval;
    static volatile u64 _allocated_bytes;

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);
    static void recordPromotion(void* ucontext, StackFrame& frame, uintptr_t oop);

  public:
    const char* name() {
        return "alloc";
    }

    const char* units() {
        return "bytes";
    }

    CStack cstack() {
        return CSTACK_NO;
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();
};

#endif // _SURVIVORTRACER_H
