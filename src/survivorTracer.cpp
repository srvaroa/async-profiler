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

#include <stdio.h>
#include "survivorTracer.h"
#include "profiler.h"
#include "vmStructs.h"


// full symbol:  _ZN20G1ParScanThreadState22copy_to_survivor_spaceE11InCSetStateP7oopDescP11markOopDesc
Trap SurvivorTracer::_copy_to_survivor("_ZN20G1ParScanThreadState22copy_to_survivor_space");

// Called whenever our breakpoint trap is hit
void SurvivorTracer::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    StackFrame frame(ucontext);

    // PC points either to BREAKPOINT instruction or to the next one
    if (frame.pc() - (uintptr_t)_copy_to_survivor._entry <= sizeof(instruction_t)) {
        // copy_to_survivor_space(InCSetState const state, oop const old, markOop const old_mark)
        // recordPromotion(ucontext, frame, frame.arg1());
        Profiler::_instance.recordSample(ucontext, 1, BCI_SYMBOL, NULL);
    }
}

void SurvivorTracer::recordPromotion(void* ucontext, StackFrame& frame, uintptr_t oopDesc) {
    // Leave the trapped function by simulating "ret" instruction
    frame.ret();

    if (VMStructs::hasClassNames()) {
        printf("Has class names\n");
        /*
         * TODO: figure out how to extract the Klass from here,
         * recording a 1 for now.
        printf("HELLO class %p", oopDesc + 8);
        VMSymbol* symbol = VMKlass::fromHandle(oopDesc)->name();
        printf("HELLO %p", symbol);
        Profiler::_instance.recordSample(ucontext, 1, BCI_SYMBOL, symbol);
        */
        Profiler::_instance.recordSample(ucontext, 1, BCI_SYMBOL, NULL);
    } else {
        Profiler::_instance.recordSample(ucontext, 1, BCI_SYMBOL, NULL);
    }
}

Error SurvivorTracer::check(Arguments& args) {
    NativeCodeCache* libjvm = VMStructs::libjvm(); 
    if (!(_copy_to_survivor.resolve(libjvm))) {
        return Error("No SurvivorTracer symbols found. Are JDK debug symbols installed?");
    }

    return Error::OK;
}

Error SurvivorTracer::start(Arguments& args) {
    Error error = check(args);
    if (error) {
        return error;
    }

    OS::installSignalHandler(SIGTRAP, signalHandler);

    _copy_to_survivor.install();

    return Error::OK;
}

void SurvivorTracer::stop() {
    _copy_to_survivor.uninstall();
}
