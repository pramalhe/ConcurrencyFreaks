/**
 * ****************************************************************************
 * Copyright (c) 2014, Pedro Ramalhete and Andreia Correia All rights
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
package com.concurrencyfreaks.locks;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;



/**
 * Thread-ID EXchange Mutual Exclusion Lock (Tidex Mutex)
 * 
 * This is a mutual exclusion lock which we discovered (or so it seams) and
 * is inspired by the Ticket Lock, but uses getAndSet() instead of getAndAdd().
 * The ingress and egress are not counters. The ingress represents thread ids 
 * of the threads trying to acquire the lock, and the egress holds the thread
 * id of the thread that currently has the lock (or has just released it).
 * Each thread has two unique identifiers for which we use the thread id 
 * and its negative value.
 * There is also a regular variable that holds state between lock() and 
 * unlock() which we named {@code nextEgress}. 
 * 
 * Notice that the initial decision of whether to spin or enter the critical
 * section is reached in a wait-free way on x86 or other systems for which
 * getAndSet() is implemented with a single atomic instruction (XCHG in
 * the x86 case). This means this lock is starvation-free on x86.
 * 
 * More details on this post:
 * http:...
 * 
 * TODO: Make a reentrant version
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class TidexMutex implements Lock {
    private static final int INVALID_TID = 0;
    private static final int NCPU = Runtime.getRuntime().availableProcessors();
    private static final int MAX_SPIN = (NCPU > 1) ? 1 << 10 : 0;
    
    private final AtomicLong ingress = new AtomicLong(INVALID_TID);
    
    @sun.misc.Contended
    private volatile long egress = INVALID_TID;
    
    private long nextEgress = INVALID_TID;
    
    
    @Override
    public void lock() {
        long mytid = Thread.currentThread().getId();
        if (egress == mytid) mytid = -mytid;
        final long prevtid = ingress.getAndSet(mytid);
        while (egress != prevtid) {     
            // Spin for a while and then yield
            for (int k = MAX_SPIN; k > 0; k--) {
                if (egress == prevtid) {
                    // Lock has been acquired
                    nextEgress = mytid;
                    return;
                }
            }
            Thread.yield(); 
        }
        // Lock has been acquired
        nextEgress = mytid;
    }

    
    @Override
    public void unlock() {
        egress = nextEgress;
    }
    
    
    @Override
    public void lockInterruptibly() throws InterruptedException {
        // Not supported
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean tryLock() {
        final long localEgress;
        if ((localEgress = egress) != ingress.get()) return false;
        long mytid = Thread.currentThread().getId();
        if (localEgress == mytid) mytid = -mytid;
        // localEgress is the same as current ingress
        if (!ingress.compareAndSet(localEgress, mytid)) return false; 
        // Lock has been acquired
        nextEgress = mytid;
        return true;
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
