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

#ifndef _URCU_TWO_PHASE_H_
#define _URCU_TWO_PHASE_H_

#include <atomic>
#include <thread>


// Our own userspace implementation of RCU that allows for concurrent calls to rcu_synchronize(),
// in other words, threads calling rcu_synchronize() can "shared the grace period".
template<typename RI>
class URCUTwoPhase {
    alignas(128) std::atomic<int64_t> updaterVersion {0};
    alignas(128) RI readIndicator[2];

public:
    int rcu_read_lock(const int tid=0) {
        const int index = (int)(updaterVersion.load() & 1);
        // Branch prediction is faster than readIndicator[index].arrive(tid);
        if (index == 0) {
            readIndicator[0].arrive(tid);
        } else {
            readIndicator[1].arrive(tid);
        } 
        return index;
    }

    void rcu_read_unlock(const int index, const int tid=0) {
        readIndicator[index].depart(tid);
    }

    void synchronize_rcu() {
        const int64_t currUV = updaterVersion.load();
        const int64_t nextUV = (currUV+1);
        while (!readIndicator[(int)(nextUV&1)].isEmpty()) { // spin
            if (updaterVersion.load() > nextUV) return;
            if (updaterVersion.load() == nextUV) break;
        }
        if (updaterVersion.load() == currUV) {
            auto tmp = currUV;
            updaterVersion.compare_exchange_strong(tmp, nextUV);
        }
        while (!readIndicator[(int)(currUV&1)].isEmpty()) { // spin
            if (updaterVersion.load() > nextUV) return;
        }
    }
};

#endif
