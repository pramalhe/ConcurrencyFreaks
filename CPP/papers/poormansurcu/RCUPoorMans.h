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

#ifndef _RCU_POOR_MANS_H_
#define _RCU_POOR_MANS_H_

#include <pthread.h>
#include "RCUBase.h"

namespace RCU {

/**
 * <h1> Poor Man's RCU </h1>
 * This is an RCU implementation on top of pthreads, which means you can safely
 * use it in any system with pthreads, with C++ versions previous to C++11.
 * It is easy to port this code to C99.
 *
 * rcu_read_lock()     - Lock-Free
 * rcu_read_unlock()   - Wait-Free
 * synchronize_rcu()   - Blocking
 *
 * Notice that rcu_read_lock() and rcu_read_unlock() are _not_ reentrant, but
 * they can be implemented as such with the help of thread-local-storage.
 *
 * The reason in rcu_synchronize() we lock rwlock2 first and then rwlock1,
 * while in rcu_read_lock() we try first on rwlock1 and then on rwlock1, is
 * so that there is a probabilistic slightly reduction in contention, but
 * reversing the order in any of these two methods is also valid.
 *
 * In RCU, threads can have one of two roles: they're either an "Arriver"
 * or a "Toggler". The Arrivers call rcu_read_lock() and rcu_read_unlock(),
 * while the Togglers call synchronize_rcu().
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
class RCUPoorMans : public RCUBase {

private:
    pthread_mutex_t  _togglerMutex  __attribute__(( aligned(64) ));
    pthread_rwlock_t _rwlock1       __attribute__(( aligned(64) ));
    pthread_rwlock_t _rwlock2       __attribute__(( aligned(64) ));

public:
    RCUPoorMans() {
        pthread_mutex_init(&_togglerMutex, NULL);
        pthread_rwlock_init(&_rwlock1, NULL);
        pthread_rwlock_init(&_rwlock2, NULL);
    }

    ~RCUPoorMans() {
        pthread_mutex_destroy(&_togglerMutex);
        pthread_rwlock_destroy(&_rwlock1);
        pthread_rwlock_destroy(&_rwlock2);
    }

    /**
     * Marks that a new Arriver has arrived.
     *
     * Progress Condition: Lock-Free
     */
    const int read_lock(void) {
        while (1) {
            if (pthread_rwlock_tryrdlock(&_rwlock1) == 0) {
                return 1;
            }
            if (pthread_rwlock_tryrdlock(&_rwlock2) == 0) {
                return 2;
            }
        }
    }

    /**
     * Marks that an Arriver has departed.
     * If you can't have easy access to the return value of rcu_read_lock(),
     * just put it on a thread-local and pass it here.
     *
     * Progress Condition: Wait-Free
     */
    void read_unlock(const int whichone) {
        if (whichone == 1) {
            pthread_rwlock_unlock(&_rwlock1);
        } else {    // which_rwlock == 2
            pthread_rwlock_unlock(&_rwlock2);
        }
    }

    /**
     * Waits for all the departure of all the Arrivers that called arrived
     * before this method was called.
     * This is meant to be called by the Toggler only.
     *
     * Progress Condition: Blocking
     */
    void synchronize(void) {
        pthread_mutex_lock(&_togglerMutex);
        pthread_rwlock_wrlock(&_rwlock2);
        pthread_rwlock_unlock(&_rwlock2);
        pthread_rwlock_wrlock(&_rwlock1);
        pthread_rwlock_unlock(&_rwlock1);
        pthread_mutex_unlock(&_togglerMutex);
    }
};
}

#endif /* _RCU_POOR_MANS_H_ */
