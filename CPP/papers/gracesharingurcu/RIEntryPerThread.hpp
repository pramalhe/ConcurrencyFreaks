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

#ifndef _READINDICATOR_ENTRY_PER_THREAD_H_
#define _READINDICATOR_ENTRY_PER_THREAD_H_

#include <atomic>
#include <thread>

/**
 * <h1> ReadIndicator with one entry per thread </h1>
 * TODO:
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
class RIEntryPerThread {

private:
    enum State { NOT_READING=0, READING=1 };
    static const int URCU_MAX_THREADS = 32;
    static const int URCU_CLPAD = (128/sizeof(std::atomic<uint64_t>));
    std::atomic<long> states[URCU_MAX_THREADS*URCU_CLPAD] alignas(128);

public:
    RIEntryPerThread() {
        for (int i = 0; i < URCU_MAX_THREADS; i++) {
            states[i*URCU_CLPAD].store(NOT_READING, std::memory_order_relaxed);
        }
    }

    void arrive(int tid) {
        states[tid*URCU_CLPAD].store(READING);
        states[tid*URCU_CLPAD].load(); // To prevent re-ordering of code below arrive() with arrive()
    }

    void depart(int tid) {
        states[tid*URCU_CLPAD].store(NOT_READING, std::memory_order_release);
    }

    bool isEmpty(void) {
        for (int tid = 0; tid < URCU_MAX_THREADS; tid ++) {
            if (states[tid*URCU_CLPAD].load(std::memory_order_acquire) == READING) {
                return false;
            }
        }
        return true;
    }
};

#endif /* _READINDICATOR_ENTRY_PER_THREAD_H_ */
