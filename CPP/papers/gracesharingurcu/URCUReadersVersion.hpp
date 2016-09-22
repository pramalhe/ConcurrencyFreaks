/******************************************************************************
 * Copyright (c) 2016, Pedro Ramalhete, Andreia Correia
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

#ifndef _URCU_READERS_VERSION_H_
#define _URCU_READERS_VERSION_H_

#include <atomic>


// Our own userspace implementation of RCU that allows for concurrent calls to rcu_synchronize(),
// in other words, threads calling rcu_synchronize() can "shared the grace period".
class URCUReadersVersion {

    static const int CLPAD = (128/sizeof(uint64_t));
    static const uint64_t NOT_READING = 0xFFFFFFFFFFFFFFFE;
    static const uint64_t UNASSIGNED =  0xFFFFFFFFFFFFFFFD;

    const int maxThreads; // Defaults to 32
    std::atomic<uint64_t> reclaimerVersion alignas(128) = { 0 };
    std::atomic<uint64_t>* readersVersion alignas(128);

public:
    URCUReadersVersion(const int maxThreads = 32) : maxThreads{maxThreads} {
        readersVersion = new std::atomic<uint64_t>[maxThreads*CLPAD];
        for (int i=0; i < maxThreads; i++) {
            readersVersion[i*CLPAD].store(UNASSIGNED, std::memory_order_relaxed);
        }
    }

    ~URCUReadersVersion() {
        delete[] readersVersion;
    }

    // Returns the index (tid) in the array
    int register_thread() {
        for (int i=0; i < maxThreads; i++) {
            if (readersVersion[i*CLPAD].load() != UNASSIGNED) continue;
            uint64_t curr = UNASSIGNED;
            if (readersVersion[i*CLPAD].compare_exchange_strong(curr, NOT_READING)) {
                 return i;
            }
        }
        std::cout << "Error: too many threads already registered\n";
    }

    // Pass the tid returned by register_thread()
    void unregister_thread(int tid)
    {
        if (readersVersion[tid*CLPAD].load() == UNASSIGNED) {
            std::cout << "Error: calling unregister_thread() with a tid that was never registered\n";
            return;
        }
        readersVersion[tid*CLPAD].store(UNASSIGNED);
    }


    void read_lock(const int tid) noexcept {
        const uint64_t rv = reclaimerVersion.load();
        readersVersion[tid*CLPAD].store(rv);
        const uint64_t nrv = reclaimerVersion.load();
        if (rv != nrv) readersVersion[tid*CLPAD].store(nrv, std::memory_order_relaxed);
    }


    void read_unlock(const int tid) noexcept {
        readersVersion[tid*CLPAD].store(NOT_READING, std::memory_order_release);
    }


    void synchronize() noexcept {
        const uint64_t waitForVersion = reclaimerVersion.load()+1;
        auto tmp = waitForVersion-1;
        reclaimerVersion.compare_exchange_strong(tmp, waitForVersion);
        for (int i=0; i < maxThreads; i++) {
            while (readersVersion[i*CLPAD].load() < waitForVersion) { } // spin
        }
    }
};

#endif
