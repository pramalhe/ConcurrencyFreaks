/******************************************************************************
 * Copyright (c) 2015, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

#ifndef _READINDICATOR_ATOMIC_COUNTER_ARRAY_H_
#define _READINDICATOR_ATOMIC_COUNTER_ARRAY_H_

#include <atomic>
#include <thread>
#include <functional>


/**
 * <h1> Atomic Counter Array ReadIndicator </h1>
 * Use an array of atomic counters to act as a ReadIndicator
 * <p>
 * Progress Conditions: <ul>
 * <li>arrive()  - O(1), Wait-Free Population Oblivious (on x86)
 * <li>depart()  - O(1), Wait-Free Population Oblivious (on x86)
 * <li>isEmpty() - O(1), Wait-Free Population Oblivious (on x86)
 * </ul>
 * Advantages: <ul>
 * <li> Requires no static assignment of threads, which means you can have as
 * many threads as needed calling arrive()/depart()
 * <li> WFPO progress conditions on x86
 * </ul>
 * <p>
 * Disadvantages: <ul>
 * <li> Memory usage
 * </ul>
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
class RIAtomicCounterArray {

private:
    static const int MAX_THREADS = 32;
    static const int CLPAD = (128/sizeof(std::atomic<uint64_t>));
    static const int COUNTER_SIZE = 3*MAX_THREADS; // Alternatively, use std::thread::hardware_concurrency()
    static std::hash<std::thread::id> hashFunc;
    alignas(128) std::atomic<uint64_t> counters[COUNTER_SIZE*CLPAD] ;

public:
    RIAtomicCounterArray() {
        for (int i=0; i < COUNTER_SIZE; i++) {
            counters[i*CLPAD].store(0, std::memory_order_relaxed);
        }
    }

    void arrive(const int notused=0) {
        const uint64_t tid = hashFunc(std::this_thread::get_id());
        const int icounter = (int)(tid % COUNTER_SIZE);
        counters[icounter*CLPAD].fetch_add(1);
    }

    void depart(const int notused=0) {
        const uint64_t tid = hashFunc(std::this_thread::get_id());
        const int icounter = (int)(tid % COUNTER_SIZE);
        counters[icounter*CLPAD].fetch_add(-1);
    }

    bool isEmpty(void) {
        for (int i = 0; i < COUNTER_SIZE; i++) {
            if (counters[i*CLPAD].load(std::memory_order_acquire) > 0) return false;
        }
        return true;
    }
};

#endif /* _READINDICATOR_ATOMIC_COUNTER_ARRAY_H_ */
