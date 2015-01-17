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
package com.concurrencyfreaks.counter;

import java.util.concurrent.atomic.AtomicLongArray;

/**
 * <h1> Resettable Linearizable Monotonic Statistical Counter </h1>
 * 
 * A statistical counter that uses atomic counters with different cache lines 
 * and aggregates them. It premits only monotic increments and the reset/clear
 * operations is linearizable, which means all three operations of this class
 * are linearizable.
 * <p>
 * Progress Conditions:
 * <ul>
 * <li> increment() - Wait-Free Population Oblivious on x86, Lock-Free otherwise
 * <li> decrement() - Wait-Free Population Oblivious on x86, Lock-Free otherwise
 * <li> sum()       - Wait-Free (Bounded by the number of cores)
 * <li> reset()     - Wait-Free Population Oblivious
 * </ul>
 * <p>
 * TODO: post
 * 
 * Performance plots and a longer description can be seen on this post about the DCLC:
 * <a href="http://concurrencyfreaks.com/2013/08/concurrency-pattern-distributed-cache.html">Distributed Cache-Line Counter</a>
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class ResLinMonStatisticalCounter {
    // Size of the counters[] array (TODO: explain the magical number <<2)
    private final static int kNumCounters = Integer.highestOneBit(Runtime.getRuntime().availableProcessors())<<1;
    
    // Size of a cache line in ints
    private final static int CACHE_LINE = 64/8;    
    
    // Stores the number of readers holding the read-lock.
    // Yes, this is volatile because of the reset() method.
    private volatile AtomicLongArray counters = new AtomicLongArray(kNumCounters*CACHE_LINE);
    
    
    /**
     * An imprecise but fast hash function (by George Marsaglia)
     * See http://www.javamex.com/tutorials/random_numbers/xorshift.shtml
     */
    private int tid2hash() {
        long x = Thread.currentThread().getId();
        x ^= (x << 21);
        x ^= (x >>> 35);
        x ^= (x << 4);
        final int idx = (int)(((kNumCounters-1) & x)*CACHE_LINE);
        return idx;
    }
    
    
    /**
     * Adds 1 to the counter.
     * <p>
     * Progress Condition: Wait-Free Population Oblivious O(1)
     * <br>Above is only true on architectures that support fetch-and-add natively,
     * and only with this fix:
     * <a href="http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=7023898">intrinsify getAndIncrement()</a>
     * <p>
     * Consistency Model: Sequentially Consistent with decrement() and sum()
     */
    public void increment() {
        counters.getAndIncrement(tid2hash());
    }
    
    
    /**
     * Subtracts 1 from the counter.
     * <p>
     * Progress Condition: Wait-Free Population Oblivious O(1)
     * <br>Above is only true on architectures that support fetch-and-add natively,
     * and only with this fix:
     * <a href="http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=7023898">intrinsify getAndIncrement()</a>
     * <p>
     * Consistency Model: Sequentially Consistent with increment() and sum()
     */
    public void decrement() {
        counters.getAndDecrement(tid2hash());
    }
    
    
    /**
     * Returns the value of the counter.
     * <p>
     * Progress Condition: Wait-Free O(N_Cores)
     * <p>
     * Consistency Model: Sequentially Consistent with increment() and decrement().
     */
    public long sum() {
        long sum = 0;
        final AtomicLongArray localCounters = counters;
        for (int idx = 0; idx < kNumCounters*CACHE_LINE; idx += CACHE_LINE) {
            sum += localCounters.get(idx); 
        }
        return sum;
    }
    
    
    /**
     * Sets the counter to zero.
     * <p>
     * Progress Condition: Wait-Free Population Oblivious
     * Unlike DCLC, this _is_ Linearizable and therefore, also sequentially 
     * consistent with the other methods of this class
     */
    public void reset() {
        counters = new AtomicLongArray(kNumCounters*CACHE_LINE);
    }    
}
