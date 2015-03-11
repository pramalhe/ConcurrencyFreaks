/******************************************************************************
 * Copyright (c) 2014-2015, Pedro Ramalhete, Andreia Correia
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

/* Thread ID EXchange Mutual Exclusion Lock (Tidex Mutex)
 *
 * This is a mutual exclusion lock which we discovered (or so it seams) and
 * is inspired by the Ticket Lock, but uses atomic_exchange() instead of
 * atomic_fetch_add().
 * The ingress and egress are not counters. The ingress represents thread ids
 * of the threads trying to acquire the lock, and the egress holds the thread
 * id of the thread that currently has the lock (or has just released it).
 * Each thread has two unique identifiers for which we use the thread id
 * and its negative value. Using the negative of a pthread_t is undefined
 * behavior according to the pthreads documentation, but in practice it
 * should be ok for all scenarios.
 * There is also a regular variable that holds state between lock() and
 * unlock() which we named {@code nextEgress}.
 *
 * Notice that the initial decision of whether to spin or enter the critical
 * section is reached in a wait-free way on x86 or other systems for which
 * atomic_exchange() is implemented with a single atomic instruction (XCHG in
 * the x86 case). This means this lock is starvation-free on x86.
 *
 * More info on this post:
 * http://concurrencyfreaks.com/2014/12/tidex-mutex-in-c11.html
 *
 * This implementation has no spinning and no padding
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
#include "tidex_mutex.h"



void tidex_mutex_init(tidex_mutex_t * self)
{
    self->nextGrant = INVALID_TID;
    atomic_store(&self->ticket, INVALID_TID);
    atomic_store(&self->grant, INVALID_TID);
}


void tidex_mutex_destroy(tidex_mutex_t * self)
{
    // Kind of unnecessary, but oh well
    atomic_store(&self->ticket, INVALID_TID);
    atomic_store(&self->grant, INVALID_TID);
}


/*
 * Locks the mutex
 * Progress Condition: Blocking
 *
 * The first load on egress can be relaxed because we're only interested
 * in finding out whether it is the same thread id as the current thread or
 * not. If it it is the same, then it is guaranteed to be up-to-date, and if
 * it is different we don't care. It can also 'seem' the same and no longer
 * be the same, which is also ok because we'll be using the negative of
 * pthread_self() when could in fact use pthread_self(), but that's not
 * a problem.
 */
void tidex_mutex_lock(tidex_mutex_t * self)
{
    long long mytid = (long long)pthread_self();
    if (atomic_load_explicit(&self->grant, memory_order_relaxed) == mytid) mytid = -mytid;
    long long prevtid = atomic_exchange(&self->ticket, mytid);
    while (atomic_load(&self->grant) != prevtid) {
        sched_yield();  // Replace this with thrd_yield() if you use <threads.h>
    }
    // Lock has been acquired
    self->nextGrant = mytid;
}


/*
 * Unlocks the mutex
 * Progress Condition: Wait-Free Population Oblivious
 */
void tidex_mutex_unlock(tidex_mutex_t * self)
{
    atomic_store(&self->grant, self->nextGrant);
}


/*
 * Tries to lock the mutex
 * Returns 0 if the lock has been acquired and EBUSY otherwise
 * Progress Condition: Wait-Free Population Oblivious
 *
 * Yes, we must use a CAS instead of an EXCHG, but it's
 * still wait-free because if the CAS fails we give up (there
 * is already another thread holding the lock).
 */
int tidex_mutex_trylock(tidex_mutex_t * self)
{
    long long localG = atomic_load(&self->grant);
    long long localT = atomic_load_explicit(&self->ticket, memory_order_relaxed);
    if (localG != localT) return EBUSY;
    long long mytid = (long long)pthread_self();
    if (localG == mytid) mytid = -mytid;
    if (!atomic_compare_exchange_strong(&self->ticket, &localT, mytid)) return EBUSY;
    // Lock has been acquired
    self->nextGrant = mytid;
    return 0;
}
