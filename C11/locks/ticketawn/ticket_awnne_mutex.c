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

/*
  * <h1>Ticket Lock with Array of Waiting Nodes</h1>
 * This is a variant of the Ticket Lock where each thread spins on its own node.
 *
 * <h2> Negative Egress variant </h2>
 * In this variant, the thread doing the unlock() uses the sign of the egress
 * to tell the next waiting thread whether it saw a waiting node or not.
 * Negative egress means that the next thread should spin on lockIsMine.
 *
 * <h2> Ticket Lock behavior </h2>
 * The Ticket Lock is described in section 2.2 by John Mellor-Crummey and
 * Michael Scott in 1991:
 * <a href="http://web.mit.edu/6.173/www/currentsemester/readings/R06-scalable-synchronization-1991.pdf">
 * http://web.mit.edu/6.173/www/currentsemester/readings/R06-scalable-synchronization-1991.pdf</a>
 * <br>This variant's behavior is a follows:
 * <ul>
 * <li> ticket == egress:   Works like a Ticket Lock
 * <li> ticket-1 == egress: Works like a ticket Lock
 * <li> ticket-2 >= egress: Uses the new mechanism
 * </ul>
 *
 * <h2> Comparison with CLH </h2>
 * This algorithm does more work than CLH so it's not that better, but in a
 * machine where there is no single instruction atomic_exchange() or it is much
 * slower than atomic_fetch_add(), this technique could be better than CLH. <br>
 * There is an advantage over CLH: In Ticket AWN there is a single awn_node_t
 * instance per thread that is shared among all instances of Ticket AWN, while
 * on the CLH there must be one node per instance per thread. This means that
 * memory wise, if an array of MAX_THREADS is used for the Ticket AWN, the
 * Ticket AWN should consume slightly less memory overall:
 * <ul>
 * <li> CLH: sizeof(node) x Number of instances x MAX_THREADS
 * <li> Ticket AWN: (sizeof(node) x MAX_THREADS) + (sizeof(ptr) x Number of instances x maxArrayWaiters)
 * </ul>
 *
 * <h2> Happens-Before </h2>
 * Notice that on {@code unlock()} we write on {@code wnode.isLocked} and then
 * on {@code egress}, while on lock() we read {@code egress} and then
 * {@code wnode.islocked}, which creates a Happens-Before:
 * <ul>
 * <li>lock(): waitersArray.store() -> egress.load() -> lockIsMine.load()
 * <li>unlock(): waitersArray.load() -> lockIsMine.store() -> egress.store()
 * <li>unlock(): waitersArray.load() -> egress.store()
 * </ul>
 * <h2> Sample scenario </h2>
 * To understand how this mechanism works, imagine a sample scenario where
 * Thread 1 (T1) gets a ticket of 10:
 * <ul>
 *  <li> egress is 10: T1 has the lock
 * <li> egress is 9: T1 will spin on egress waiting for it to become 10
 * <li> egress is 8: T1 will add its node to the waitersArray and check egress again:
 *   <li> egress is still 8: T1 will spin on lockIsMine
 *   <li> egress is now  -9: T1 will spin on egress (egress may be about to pass to 10)
 *   <li> egress is now   9: T1 will spin on egress (egress may be about to pass to 10)
 *   <li> egress is now -10: T1 will spin on lockIsMine (previous thread has seen T1's node)
 *   <li> egress is now  10: T1 has the lock
 *   </ul>
 * </ul>
 *
 * <h2> Atomic Operations </h2>
 * Notice that in Ticket AWN the only atomic operation that is not a simple
 * load or store is the atomic_fetch_add() done at the beginning of the
 * lock() method. No other atomic_fetch_add(), or atomic_exchange() or
 * atomic_compare_exchange() are done on this algorithm, and several of the
 * atomic loads and stores can be done relaxed.
 *
 * <h2> Relaxed Atomic Operations </h2>
 * There are a few loads and stores than can be done relaxed. Here is the
 * list and the justification:
 * <ul>
 * <li> In lock(), the store on wnode->lockIsMine to false can be relaxed
 *      because it will become visible on the release-store of the wnode in
 *      the array and it will only be accessible from that instant in time.
 * <li> In unlock(), the first load on egress can be relaxed because it was
 *      read last by the same thread in lock() so it is guaranteed to be up
 *      to date.
 * <li> In unlock(), the store on the self entry of the array to NULL can be
 *      relaxed because it will become visible either with the store with
 *      release on the wnode->lockIsMine to true or on the egress to ticket+1.
 * <li> In unlock(), the store in egress of -(ticket+1) can be relaxed
 *      because before it there is a load with load with acquire of the array
 *      which prevents reordering above, and the store will become visible
 *      when we do the store with release of wnode->lockIsMine to true.
 * </ul>
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
#include "ticket_awnne_mutex.h"

/*
 * Each thread has its own awn_node_t instance. The design goal is for each
 * thread waiting on the lock to be spinning on its own copy if the awn_node_t
 * instance in the lockIsMine variable, instead of all threads spinning on the
 * egress variable, thus reducing traffic on the cache-coherence system.
 */
static _Thread_local awnne_node_t tlNode;


/**
 * If you don't know what to put in maxArrayWaiters just use DEFAULT_MAX_WAITERS
 *
 * @param maxArrayWaiters Size of the array of waiter threads. We recommend
 *                        using the number of cores or at most the number of
 *                        threads expected to concurrently attempt to acquire
 *                        the lock.
 */
void ticket_awnne_mutex_init(ticket_awnne_mutex_t * self, int maxArrayWaiters)
{
    atomic_store(&self->ingress, 0);
    atomic_store(&self->egress, 0);
    self->maxArrayWaiters = maxArrayWaiters;
    self->waitersArray = (awnne_node_t **)malloc(self->maxArrayWaiters*sizeof(awnne_node_t *));
    for (int i = 0; i < self->maxArrayWaiters; i++) self->waitersArray[i] = ATOMIC_VAR_INIT(NULL);
}


void ticket_awnne_mutex_destroy(ticket_awnne_mutex_t * self)
{
    atomic_store(&self->ingress, 0);
    atomic_store(&self->egress, 0);
    free(self->waitersArray);
}

static long long get_pos_egress(ticket_awnne_mutex_t * self) {
    long long localEgress = atomic_load(&self->egress);
    return localEgress > 0 ? localEgress : -localEgress;
}

static long long get_pos_egress_relaxed(ticket_awnne_mutex_t * self) {
    long long localEgress = atomic_load_explicit(&self->egress, memory_order_relaxed);
    return localEgress > 0 ? localEgress : -localEgress;
}

/*
 * Locks the mutex
 * Progress Condition: Blocking
 *
 * Notice that in the best case scenario there will be two acquires and one
 * release barriers, in atomic_fetch_add() on ingress, and in the first
 * atomic_load() of egress.
 */
void ticket_awnne_mutex_lock(ticket_awnne_mutex_t * self)
{
    const long long ticket = atomic_fetch_add(&self->ingress, 1);
    if (atomic_load(&self->egress) == ticket) return;
    while (get_pos_egress_relaxed(self) >= ticket-1) {
        if (get_pos_egress(self) == ticket) return;
        sched_yield();
    }
    // If there is no slot to wait, spin until there is
    while ((ticket-get_pos_egress(self)) >= (self->maxArrayWaiters-1)) sched_yield();

    // There is a spot for us on the array, so place our node there
    awnne_node_t * wnode = &tlNode;
    // Reset lockIsMine from previous usages
    atomic_store_explicit(&wnode->lockIsMine, false, memory_order_relaxed);
    atomic_store(&self->waitersArray[(int)(ticket % self->maxArrayWaiters)], wnode);

    // If there is only one left before egress becomes our ticket, wait for it
    while (true) {
        const long long localEgress = atomic_load(&self->egress);
        if (get_pos_egress_relaxed(self) < ticket-1 || -localEgress == ticket) break; // go spin on lockIsMine
        if (localEgress == ticket) return; // egress was positive and matches. Lock acquired
        sched_yield();
    }
    // Spin on our own cache line waiting for the lock
    while (!atomic_load(&wnode->lockIsMine)) {
        sched_yield();
    }
}


/*
 * Unlocks the mutex
 * Progress Condition: Wait-Free Population Oblivious
 *
 * Notice that in this function there is only one release barrier and one acquire barrier.
 */
void ticket_awnne_mutex_unlock(ticket_awnne_mutex_t * self)
{
    long long ticket = get_pos_egress_relaxed(self);
    // Clear up our entry in the array before releasing the lock.
    atomic_store_explicit(&self->waitersArray[(int)(ticket % self->maxArrayWaiters)], NULL, memory_order_relaxed);
    // We could do this load as relaxed per se but then the store on egress of -(ticket+1) could be re-ordered to be before, and we don't want that
    awnne_node_t * wnode = atomic_load(&self->waitersArray[(int)((ticket+1) % self->maxArrayWaiters)]);
    if (wnode != NULL) {
        // We saw the node in waitersArray, so tell the thread to spin on lockIsMine by setting a negative egress
        atomic_store_explicit(&self->egress, -(ticket+1), memory_order_relaxed);
        atomic_store(&wnode->lockIsMine, true);
    } else {
        // No node was seen, so set a positive egress
        atomic_store(&self->egress, ticket+1);
    }
}


/*
 * Tries to lock the mutex
 * Returns 0 if the lock has been acquired and EBUSY otherwise
 * Progress Condition: Wait-Free Population Oblivious
 */
int ticket_awnne_mutex_trylock(ticket_awnne_mutex_t * self)
{
    long long localE = atomic_load(&self->egress);
    long long localI = atomic_load_explicit(&self->ingress, memory_order_relaxed);
    if (localE != localI) return EBUSY;
    if (!atomic_compare_exchange_strong(&self->ingress, &localI, self->ingress+1)) return EBUSY;
    // Lock has been acquired
    return 0;
}

