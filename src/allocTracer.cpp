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
#include "profiler.h"
#include "vmStructs.h"


// JDK 7-9
Trap AllocTracer::_in_new_tlab("_ZN11AllocTracer33send_allocation_in_new_tlab_event");
Trap AllocTracer::_outside_tlab("_ZN11AllocTracer34send_allocation_outside_tlab_event");
// JDK 10+
Trap AllocTracer::_in_new_tlab2("_ZN11AllocTracer27send_allocation_in_new_tlab");
Trap AllocTracer::_outside_tlab2("_ZN11AllocTracer28send_allocation_outside_tlab");

u64 AllocTracer::_interval;
volatile u64 AllocTracer::_allocated_bytes;


// Called whenever our breakpoint trap is hit
void AllocTracer::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    StackFrame frame(ucontext);

    // PC points either to BREAKPOINT instruction or to the next one
    if (frame.pc() - (uintptr_t)_in_new_tlab._entry <= sizeof(instruction_t)) {
        // send_allocation_in_new_tlab_event(KlassHandle klass, size_t tlab_size, size_t alloc_size)
        recordAllocation(ucontext, frame, frame.arg0(), frame.arg1(), false);
    } else if (frame.pc() - (uintptr_t)_outside_tlab._entry <= sizeof(instruction_t)) {
        // send_allocation_outside_tlab_event(KlassHandle klass, size_t alloc_size);
        recordAllocation(ucontext, frame, frame.arg0(), frame.arg1(), true);
    } else if (frame.pc() - (uintptr_t)_in_new_tlab2._entry <= sizeof(instruction_t)) {
        // send_allocation_in_new_tlab(Klass* klass, HeapWord* obj, size_t tlab_size, size_t alloc_size, Thread* thread)
        recordAllocation(ucontext, frame, frame.arg0(), frame.arg2(), false);
    } else if (frame.pc() - (uintptr_t)_outside_tlab2._entry <= sizeof(instruction_t)) {
        // send_allocation_outside_tlab(Klass* klass, HeapWord* obj, size_t alloc_size, Thread* thread)
        recordAllocation(ucontext, frame, frame.arg0(), frame.arg2(), true);
    }
}

void AllocTracer::recordAllocation(void* ucontext, StackFrame& frame, uintptr_t rklass, uintptr_t rsize, bool outside_tlab) {
    // Leave the trapped function by simulating "ret" instruction
    frame.ret();

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
    NativeCodeCache* libjvm = VMStructs::libjvm();
    if (!(_in_new_tlab.resolve(libjvm) || _in_new_tlab2.resolve(libjvm)) ||
        !(_outside_tlab.resolve(libjvm) || _outside_tlab2.resolve(libjvm))) {
        return Error("No AllocTracer symbols found. Are JDK debug symbols installed?");
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
    _in_new_tlab2.install();
    _outside_tlab2.install();

    return Error::OK;
}

void AllocTracer::stop() {
    _in_new_tlab.uninstall();
    _outside_tlab.uninstall();
    _in_new_tlab2.uninstall();
    _outside_tlab2.uninstall();
}
