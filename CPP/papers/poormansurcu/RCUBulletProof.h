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

#ifndef _RCU_BULLET_PROOF_H_
#define _RCU_BULLET_PROOF_H_

#ifdef URCU_BULLET_PROOF_LIB
#include <urcu-bp.h>
#endif
#include "RCUBase.h"

namespace RCU {

/**
 * Wrapper for URCU library's Bullet Proof
 * https://github.com/urcu/userspace-rcu#usage-of-liburcu-bp
 */
class RCUBulletProof : public RCUBase {

public:

    RCUBulletProof() {
        // rcu_init() is a no-op
#ifndef URCU_BULLET_PROOF_LIB
        std::cout << "---  WARNING:  Using a fake RCU Bullet Proof --- \n";
#endif

    }

    ~RCUBulletProof() {
    }

    /**
     * Marks that a new Arriver has arrived.
     *
     * Progress Condition: Lock-Free
     */
    const int read_lock(void) {
#ifdef URCU_BULLET_PROOF_LIB
        ::rcu_read_lock();
#endif
        return 0;
    }

    /**
     * Marks that an Arriver has departed.
     * If you can't have easy access to the return value of rcu_read_lock(),
     * just put it on a thread_local and pass it here.
     *
     * Progress Condition: Wait-Free
     */
    void read_unlock(const int whichone) {
#ifdef URCU_BULLET_PROOF_LIB
        ::rcu_read_unlock();
#endif
    }

    /**
     * Waits for all the departure of all the Arrivers that called arrived
     * before the _versionIndex changed.
     * This is meant to be called by the Toggler only.
     *
     * Progress Condition: Blocking
     */
    void synchronize(void) {
#ifdef URCU_BULLET_PROOF_LIB
        ::synchronize_rcu();
#endif
    }
};
}

#endif /* _RCU_POOR_MANS_H_ */
