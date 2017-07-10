/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
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

#ifndef _RISTATIC_H_
#define _RISTATIC_H_

#include <atomic>
#include <iostream>
#include <cstdint>


// TODO: change all calls that need the tid to use a function argument
// TODO: use std::vector instead of arrays for the retired objects (keep the padding)
class RIStaticPerThread {

private:
    const int maxThreads;
    alignas(128) std::atomic<uint64_t>* states;

    static const uint64_t NOT_READING = 0;
    static const uint64_t READING = 1;
    static const int CLPAD = 128/sizeof(uint64_t);

public:
    RIStaticPerThread(int maxThreads) : maxThreads{maxThreads} {
        states = new std::atomic<uint64_t>[maxThreads*CLPAD];
        for (int tid = 0; tid < maxThreads; tid++) {
            states[tid*CLPAD].store(NOT_READING, std::memory_order_relaxed);
        }
    }

    ~RIStaticPerThread() {
        delete[] states;
    }

    // Will attempt to pass all current READING states to
    inline void abortRollback() noexcept {
        for (int tid = 0; tid < maxThreads; tid++) {
            if (states[tid*CLPAD].load() != READING) continue;
            uint64_t read = READING;
            states[tid*CLPAD].compare_exchange_strong(read, READING+1);
        }
    }

    // Returns true if the arrival was successfully rollbacked.
    // If there was a writer changing the state to READING+1 then it will
    // return false, meaning that the arrive() is still valid and visible.
    inline bool rollbackArrive(const int tid) noexcept {
        return (states[tid*CLPAD].fetch_add(-1) == READING);
    }

    inline void arrive(const int tid) noexcept {
        states[tid*CLPAD].store(READING);
    }

    inline void depart(const int tid) noexcept {
        states[tid*CLPAD].store(NOT_READING); // Making this "memory_order_release" will cause overflows!
    }

    inline bool isEmpty() noexcept {
        for (int tid = 0; tid < maxThreads; tid++) {
            if (states[tid*CLPAD].load() != NOT_READING) return false;
        }
        return true;
    }
};

#endif /* RISTATIC_H */
