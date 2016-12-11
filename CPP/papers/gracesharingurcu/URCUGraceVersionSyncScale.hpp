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

#ifndef _URCU_GRACE_VERSION_SCALE_H_
#define _URCU_GRACE_VERSION_SCALE_H_

#include <atomic>
#include <thread>



// Our own userspace implementation of RCU that allows for concurrent calls to rcu_synchronize(),
// in other words, threads calling rcu_synchronize() can "shared the grace period".
class URCUGraceVersionSyncScale {

    static const int URCU_MAX_THREADS = 32;
    static const int CLPAD = (128/sizeof(std::atomic<int64_t>));
    static const int COUNTER_SIZE = 2*URCU_MAX_THREADS; // Alternatively, use std::thread::hardware_concurrency()
    static const int64_t NOT_READING = std::numeric_limits<int64_t>::max();
    alignas(128) std::atomic<int64_t> updaterVersion[URCU_MAX_THREADS*CLPAD] ;
    alignas(128) std::atomic<int64_t> readersVersion[URCU_MAX_THREADS*CLPAD];

    const int64_t getUpdaterVersion() const {
        int64_t sum = 0;
        for (int i=0; i < URCU_MAX_THREADS; i++) {
            sum += updaterVersion[i*CLPAD].load(std::memory_order_acquire);
        }
        return sum;
    }

public:
    URCUGraceVersionSyncScale() {
        for (int it=0; it < URCU_MAX_THREADS; it++) {
            updaterVersion[it*CLPAD].store(0, std::memory_order_relaxed);
            readersVersion[it*CLPAD].store(NOT_READING, std::memory_order_relaxed);
        }
    }

    void rcu_read_lock(const int tid) {  // rcu_read_lock()
        const int64_t rv = getUpdaterVersion();
        readersVersion[tid*CLPAD].store(rv);
        const int64_t nrv = getUpdaterVersion();
        if (rv != nrv) readersVersion[tid*CLPAD].store(nrv, std::memory_order_relaxed);
    }

    void rcu_read_unlock(const int tid) { // rcu_read_unlock()
        readersVersion[tid*CLPAD].store(NOT_READING, std::memory_order_release);
    }

    void synchronize_rcu(const int tid) {
        const int64_t waitForVersion = getUpdaterVersion()+1;
        auto current = updaterVersion[tid*CLPAD].load(std::memory_order_relaxed);
        updaterVersion[tid*CLPAD].store(current+1);
        for (int i=0; i < URCU_MAX_THREADS; i++) {
            while (readersVersion[i*CLPAD].load() < waitForVersion) { } // spin
        }
    }
};

#endif
