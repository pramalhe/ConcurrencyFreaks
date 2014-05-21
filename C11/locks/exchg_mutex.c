/******************************************************************************
 * Copyright (c) 2014, Pedro Ramalhete, Andreia Correia
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

/*
 * As far as we can tell, we discovered this algorithm for lock implementation,
 * but if you happen to see some prior art, we would like to know about it, so
 * leave us a comment at http://www.concurrencyfreaks.com
 *
 * Notice that this lock is NOT recursive, but it is easy to make it recursive.
 *
 * This mutual exclusion lock is "Starvation-Free", assuming that the run-time
 * can provide that kind of guarantee.
 *
 * It is possible to remove the field "mynode" from exchg_mutex_node_t, but
 * then we need to use either a thread-local variables to store them, or we
 * need to return it from exchg_mutex_lock() and pass it to
 * exchg_mutex_unlock() as argument, which is ok for some usages, but hard to
 * do for others.
 * In C++1x, it should be easy to use this technique with a "Guard" pattern
 * and then pass the operation on the object associated with the mutex through
 * a lambda.
 * The advantage of this last approach is that there are no loads, stores, or
 * false-sharing for "mynode"  because the compiler can put those variables on
 * a register, or at worse, on the current thread's stack.
 *
 * This locking mechanism has some similarities with the MCS lock because each
 * thread is spinning on the "islocked" of the previous node, which means that
 * each thread is waiting on a different cache-line, thus reducing
 * synchronization.
 * http://www.cise.ufl.edu/tr/DOC/REP-1992-71.pdf
 * The main different from the MCS lock (section 2) is that MCS may have to do
 * an Compare-And-Swap in addition to the atomic_store() in the unlock function.
 *
 * The Exchg mutex uses the (theoretically) minimum amount of synchronization
 * operations to perform a mutual exclusion lock, because:
 * - In the lock() function it has a single synchronized operation
 *   atomic_exchange() with one acquire and one release barrier.
 * - In the unlock() function is has a single synchronized operation, the
 *   atomic_store() with a release barrier.
 */
#include "exchg_mutex.h"


static exchg_mutex_node_t * exchg_mutex_create_node(char islocked)
{
    exchg_mutex_node_t * new_node = (exchg_mutex_node_t *)malloc(sizeof(exchg_mutex_node_t));
    atomic_store_explicit(&new_node->islocked, islocked, memory_order_relaxed);
    return new_node;
}


/*
 * Initializes the mutex, creating a sentinel node.
 *
 * Progress Condition: Wait-Free Population Oblivious
 */
void exchg_mutex_init(exchg_mutex_t * self)
{
    // We create the first sentinel node unlocked, with islocked=0
    exchg_mutex_node_t * node = exchg_mutex_create_node(0);
    self->mynode = node;
    atomic_store(&self->tail, node);
}


/*
 * Destroy the mutex, clearing all memory.
 * You must be sure that there are no other threads currently holding
 * the lock or attempting to.
 *
 * Progress Condition: Wait-Free Population Oblivious
 */
void exchg_mutex_destroy(exchg_mutex_t * self)
{
    free(atomic_load(&self->tail));
}


/*
 * Locks the mutex for the current thread. Will wait for other threads
 * that did the atomic_exchange() before this one.
 *
 * Progress Condition: Blocking
 */
void exchg_mutex_lock(exchg_mutex_t * self)
{
    // Create the new node locked by default, setting islocked=1
    exchg_mutex_node_t *mynode = exchg_mutex_create_node(1);
    exchg_mutex_node_t *prev = atomic_exchange(&self->tail, mynode);

    // This thread's node is now in the queue, so wait until it is its turn
    char prev_islocked = atomic_load_explicit(&prev->islocked, memory_order_relaxed);
    if (prev_islocked) {
        while (prev_islocked) {
            sched_yield();  // Replace this with thrd_yield() if you use <threads.h>
            prev_islocked = atomic_load(&prev->islocked);
        }
    }
    // This thread has acquired the lock on the mutex and it is now safe to
    // cleanup the memory of the previous node.
    free(prev);

    // Store mynode for exchg_mutex_unlock() to use. We could replace
    // this with a thread-local, not sure which is faster.
    self->mynode = mynode;
}


/*
 * Unlocks the mutex. Assumes that the current thread holds the lock on the
 * mutex.
 *
 * Progress Condition: Wait-Free Population Oblivious
 */
void exchg_mutex_unlock(exchg_mutex_t * self)
{
    // We assume that if this function was called, it is because this thread is
    // currently holding the lock, which means that self->mynode is pointing to
    // the current thread's mynode.
    if (self->mynode == NULL) {
        // ERROR: This will occur if unlock() is called without a lock()
        return;
    }
    atomic_store(&self->mynode->islocked, 0);
}

