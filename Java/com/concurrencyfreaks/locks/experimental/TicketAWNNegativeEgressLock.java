/**
 * ****************************************************************************
 * Copyright (c) 2014-2015, Pedro Ramalhete and Andreia Correia All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. * Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials provided
 * with the distribution. * Neither the name of the author nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *****************************************************************************
 */
package com.concurrencyfreaks.locks.experimental;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReferenceArray;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;

/**
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
 * machine where there is no getAndSet() or it is much slower than 
 * getAndAdd(), this technique could be better than CLH (although I'm not 
 * sure such a machine even exists!). <br>
 * There is an advantage over CLH: In Ticket AWN there is a single WaitingNode
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
 * To understand how the negative/positive egress mechanism works, imagine a 
 * sample scenario where Thread 1 (T1) gets a ticket of 10:
 *<ul> 
 * <li> egress is 10: T1 has the lock
 * <li> egress is 9: T1 will spin on egress waiting for it to become 10
 * <li> egress is 8: T1 will add its node to the waitersArray and check egress again:
 *   <ul>
 *   <li> egress is still 8: T1 will spin on isLocked
 *   <li> egress is now  -9: T1 will spin on egress (egress may be about to pass to 10)
 *   <li> egress is now   9: T1 will spin on egress (egress may be about to pass to 10)
 *   <li> egress is now -10: T1 will spin on isLocked (previous thread has seen T1's node)
 *   <li> egress is now  10: T1 has the lock
 *   </ul>
 * </ul>
 * 
 * <h2> On Park() and Unpark() </h2>
 * We can add {@code LockSupport.park()} and {@code unpark()} to this mechanism
 * by adding a {@code final Thread} reference in {@code WaitingNode} with the 
 * {@code Thread.currentThread()} of the thread owing the {@code WaitingNode} 
 * instance. We can then call {@code park()} in the lockIsMine loop
 * in method {@code lock()} and call {@code unpark()} after setting 
 * {@code wnode.LockIsMine} to true. <br>
 * Alternatively, we can even completely skip the {@code WaitingNode} and 
 * {@code LockIsMine} and make the {@code waitersArray} be of type 
 * {@code AtomicReferenceArray<Thread>} where the waiting thread puts the 
 * reference to itself before calling {@code park()} and then the thread 
 * calling {@code unlock()} will call {@code unpark(thread)} where 
 * {@code thread} is obtained from the {@code waitersArray} directly.
 * 
 * <h2> Atomic Operations </h2>
 * Notice that in Ticket AWN the only atomic operation that is not a simple 
 * load or store is the getAndIncrement() done at the beginning of the
 * lock() method. No other CAS, EXHG, or XADD are done on this algorithm,
 * and many of the atomic loads and stores can be done relaxed.
 * <p>
 * TODO: Implement trylock()
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class TicketAWNNegativeEgressLock implements Lock {
    private static final int NCPU = Runtime.getRuntime().availableProcessors();
    
    static class WaitingNode {
        volatile boolean lockIsMine = false;
    }
    
    /*
     * Each thread has its own WaitingNode instance. The design goal is for each
     * thread waiting on the lock to be spinning on its own copy if the WaitingNode
     * instance in the lockIsMine variable, instead of all threads spinning on the
     * egress variable, thus reducing traffic on the cache-coherence system.
     */    
    private static final ThreadLocal<WaitingNode> tlNode = new ThreadLocal<WaitingNode>();
    
    private final int maxArrayWaiters;
    private final AtomicLong ingress = new AtomicLong(0);
    @sun.misc.Contended
    private volatile long egress = 0;
    private final AtomicReferenceArray<WaitingNode> waitersArray;
    
    
    public TicketAWNNegativeEgressLock() {
        maxArrayWaiters = NCPU+2;
        waitersArray = new AtomicReferenceArray<WaitingNode>(maxArrayWaiters);
    }
    
    public TicketAWNNegativeEgressLock(int maxArrayWaiters) {
        this.maxArrayWaiters = maxArrayWaiters;
        waitersArray = new AtomicReferenceArray<WaitingNode>(maxArrayWaiters);
    }
    
    
    private final long getPosEgress() { 
        long localEgress = egress;
        return localEgress > 0 ? localEgress : -localEgress;
    }
    
    
    @Override
    public void lock() {
        final long ticket = ingress.getAndIncrement();
        if (egress == ticket) return;
        while (getPosEgress() >= ticket-1) { if (getPosEgress() == ticket) return; Thread.yield(); }
        // If there is no slot to wait, spin until there is
        while (ticket-getPosEgress() >= maxArrayWaiters) Thread.yield();

        // There is a spot for us on the array, so place our node there
        WaitingNode wnode = tlNode.get();
        if (wnode == null) tlNode.set(wnode = new WaitingNode());
        wnode.lockIsMine = false; // Reset from previous usages
        waitersArray.set((int)(ticket % maxArrayWaiters), wnode);

        // If there is only one left before egress becomes our ticket, wait for it
        while (true) {
            final long localEgress = egress;
            if (getPosEgress() < ticket-1 || -localEgress == ticket) break; // go spin on lockIsMine
            if (localEgress == ticket) return; // egress was positive and matches. Lock acquired
            Thread.yield(); 
        }
        // Spin on our own cache line waiting for the lock
        while (!wnode.lockIsMine) {
            Thread.yield();
            // If we wanted to, we could call park() here
        }
    }

    
    @Override
    public void unlock() {
        long ticket = getPosEgress();
        // Clear up our entry in the array before releasing the lock. Should be a relaxed store.
        waitersArray.set((int)(ticket % maxArrayWaiters), null);
        final WaitingNode wnode = waitersArray.get((int)((ticket+1) % maxArrayWaiters));
        if (wnode != null) {
            // We saw the node in waitersArray, so tell the thread to spin on lockIsMine by setting a negative egress
            egress = -(ticket+1); // This can be a relaxed store
            wnode.lockIsMine = true;
            // If we use Park/Unpark, then here is the place to call unpark(wnode.thread)
        } else {
            // No node was seen, so set a positive egress
            egress = ticket+1;
        }
    }
    
    
    @Override
    public void lockInterruptibly() throws InterruptedException {
        // Not supported
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean tryLock() {
        // Not supported
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean tryLock(long time, TimeUnit unit)
            throws InterruptedException {
        // Not supported
        throw new UnsupportedOperationException();
    }

    @Override
    public Condition newCondition() {
        // Not supported
        throw new UnsupportedOperationException();
    }    
}
