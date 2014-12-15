/******************************************************************************
 * Copyright (c) 2013, Pedro Ramalhete, Andreia Correia
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

import java.lang.reflect.Field;


/**
 * A counter that uses atomic counters with different cache lines and 
 * aggregates them, using a relaxed load optimization.
 * <p>
 * Performance plots and a longer description can be seen on this post:
 * <a href="http://concurrencyfreaks.com/2013/08/concurrency-pattern-distributed-cache.html">Distributed Cache-Line Counter</a>
 * Relax optimization at this post:
 * <a href="http://concurrencyfreaks.com/2014/12/relaxed-atomics-on-array-of-counters.html">Relaxed atomics on array of counters</a>
 * <p>
 * Very fast for increment()/decrement() operations, but slow for sum(), i.e. slower than LongAdder.
 * <p>
 * TODO: Explain the magical number 4 (<<2) in kNumCounters
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class DistributedCacheLineCounterRelax {
    // Size of the counters[] array (TODO: explain the magical number <<2)
    private final static int kNumCounters = Integer.highestOneBit(Runtime.getRuntime().availableProcessors())<<2;
    
    // Size of a cache line in longs
    private final static int CACHE_LINE = 64/8;    
    
    // Stores the number of readers holding the read-lock 
    private final long[] counters = new long[kNumCounters*CACHE_LINE];
    
    // Used by the relaxed optimization to insert the proper fences
    private volatile long aVolatileLoad = 0;

    private static final sun.misc.Unsafe UNSAFE;
            
    static {
         try {
             Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
             f.setAccessible(true);
             UNSAFE = (sun.misc.Unsafe) f.get(null);
             int scale = UNSAFE.arrayIndexScale(long[].class);
             if ((scale & (scale - 1)) != 0)
                 throw new Error("data type scale not a power of two");
             shift = 31 - Integer.numberOfLeadingZeros(scale);
         } catch (Exception e) {
             System.out.println("In Eclipse, add -Xbootclasspath/a:./bin/ to fix this exception\n");
             throw new Error(e);
         }   
    }        
    
    private static final int base = UNSAFE.arrayBaseOffset(long[].class);
    private static final int shift;

    private long checkedByteOffset(int i) {
        if (i < 0 || i >= counters.length)
            throw new IndexOutOfBoundsException("index " + i);
        return byteOffset(i);
    }

    private static long byteOffset(int i) {
        return ((long) i << shift) + base;
    }
    
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
        UNSAFE.getAndAddLong(counters, checkedByteOffset(tid2hash()), 1);
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
        UNSAFE.getAndAddLong(counters, checkedByteOffset(tid2hash()), -1);
    }
    
    
    /**
     * Returns the value of the counter.
     * We read the first entry in the array with a volatile load, thus preventing 
     * regular loads from being re-ordered up, and in the end we do a loadFence()
     * to prevent regular loads from being re-ordered downwards.
     * <p>
     * Progress Condition: Wait-Free O(N_Cores)
     * <p>
     * Consistency Model: Sequentially Consistent with increment() and decrement().
     */
    public long sum() {
        long sum = aVolatileLoad;
        for (int idx = 0; idx < kNumCounters*CACHE_LINE; idx += CACHE_LINE) {
            sum += counters[idx]; 
        }
        UNSAFE.loadFence();
        return sum;
    }
    
    
    /**
     * Sets the counter to zero.
     * This is not Sequentially Consistent with any of the remaining functions.
     */
    public void clear() {
        for (int idx = 0; idx < kNumCounters*CACHE_LINE; idx += CACHE_LINE) {
            counters[idx] = 0; 
        }
        UNSAFE.storeFence();
    }
}
