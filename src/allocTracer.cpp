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

#include "allocTracer.h"
#include "os.h"
#include "profiler.h"
#include "stackFrame.h"
#include "vmStructs.h"


int AllocTracer::_trap_kind;
Trap AllocTracer::_in_new_tlab;
Trap AllocTracer::_outside_tlab;

u64 AllocTracer::_interval;
volatile u64 AllocTracer::_allocated_bytes;


// Called whenever our breakpoint trap is hit
void AllocTracer::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    StackFrame frame(ucontext);
    bool outside_tlab;

    // PC points either to BREAKPOINT instruction or to the next one
    if (frame.pc() - (uintptr_t)_in_new_tlab.entry() <= sizeof(instruction_t)) {
        outside_tlab = false;
    } else if (frame.pc() - (uintptr_t)_outside_tlab.entry() <= sizeof(instruction_t)) {
        outside_tlab = true;
    } else {
        // Not our trap
        return;
    }

    // Leave the trapped function by simulating "ret" instruction
    frame.ret();

    if (_trap_kind == 1) {
        // send_allocation_in_new_tlab(Klass* klass, HeapWord* obj, size_t tlab_size, size_t alloc_size, Thread* thread)
        // send_allocation_outside_tlab(Klass* klass, HeapWord* obj, size_t alloc_size, Thread* thread)
        recordAllocation(ucontext, frame.arg0(), frame.arg2(), outside_tlab);
    } else {
        // send_allocation_in_new_tlab_event(KlassHandle klass, size_t tlab_size, size_t alloc_size)
        // send_allocation_outside_tlab_event(KlassHandle klass, size_t alloc_size);
        recordAllocation(ucontext, frame.arg0(), frame.arg1(), outside_tlab);
    }
}

void AllocTracer::recordAllocation(void* ucontext, uintptr_t rklass, uintptr_t rsize, bool outside_tlab) {
    if (_interval) {
        // Do not record allocation unless allocated at least _interval bytes
        while (true) {
            u64 prev = _allocated_bytes;
            u64 next = prev + rsize;
            if (next < _interval) {
                if (__sync_bool_compare_and_swap(&_allocated_bytes, prev, next)) {
                    return;
                }
            } else {
                if (__sync_bool_compare_and_swap(&_allocated_bytes, prev, next % _interval)) {
                    break;
                }
            }
        }
    }

    if (VMStructs::hasClassNames()) {
        VMSymbol* symbol = VMKlass::fromHandle(rklass)->name();
        if (outside_tlab) {
            // Invert the last bit to distinguish jmethodID from the allocation in new TLAB
            Profiler::_instance.recordSample(ucontext, rsize, BCI_SYMBOL_OUTSIDE_TLAB, (jmethodID)((uintptr_t)symbol ^ 1));
        } else {
            Profiler::_instance.recordSample(ucontext, rsize, BCI_SYMBOL, (jmethodID)symbol);
        }
    } else {
        Profiler::_instance.recordSample(ucontext, rsize, BCI_SYMBOL, NULL);
    }
}

Error AllocTracer::check(Arguments& args) {
    if (_in_new_tlab.entry() != NULL && _outside_tlab.entry() != NULL) {
        return Error::OK;
    }

    NativeCodeCache* libjvm = VMStructs::libjvm();
    const void* ne;
    const void* oe;

    if ((ne = libjvm->findSymbolByPrefix("_ZN11AllocTracer27send_allocation_in_new_tlab")) != NULL &&
        (oe = libjvm->findSymbolByPrefix("_ZN11AllocTracer28send_allocation_outside_tlab")) != NULL) {
        _trap_kind = 1;  // JDK 10+
    } else if ((ne = libjvm->findSymbolByPrefix("_ZN11AllocTracer33send_allocation_in_new_tlab_event")) != NULL &&
               (oe = libjvm->findSymbolByPrefix("_ZN11AllocTracer34send_allocation_outside_tlab_event")) != NULL) {
        _trap_kind = 2;  // JDK 7-9
    } else {
        return Error("No AllocTracer symbols found. Are JDK debug symbols installed?");
    }

    if (!_in_new_tlab.assign(ne) || !_outside_tlab.assign(oe)) {
        return Error("Unable to install allocation trap");
    }

    return Error::OK;
}

Error AllocTracer::start(Arguments& args) {
    Error error = check(args);
    if (error) {
        return error;
    }

    _interval = args._interval;
    _allocated_bytes = 0;

    OS::installSignalHandler(SIGTRAP, signalHandler);

    _in_new_tlab.install();
    _outside_tlab.install();

    return Error::OK;
}

void AllocTracer::stop() {
    _in_new_tlab.uninstall();
    _outside_tlab.uninstall();
}
