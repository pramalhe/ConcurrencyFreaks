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
package com.concurrencyfreaks.readindicators;

import java.lang.reflect.Field;

// A ReadIndicator that uses an array of counters (Distributed Cache Line Counter) with relax optimization
// Relax optimization at this post:
//<a href="http://concurrencyfreaks.com/2014/12/relaxed-atomics-on-array-of-counters.html">Relaxed atomics on array of counters</a>
public class RIDistributedCacheLineCounter implements ReadIndicator {  
    // Size of the counters[] array
    private final int numCounters;
    
    // Size of a cache line in longs
    private static final int CACHE_LINE = 64/8;    

    private final long[] counters;
    
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
        final int idx = (int)(((numCounters-1) & x)*CACHE_LINE);
        return idx;
    }
    

    /**
     * 
     * @param numCounters Must be a power of 2
     */
    public RIDistributedCacheLineCounter(int numCounters) {
        this.numCounters = Integer.highestOneBit(numCounters);
        counters = new long[this.numCounters*CACHE_LINE];
    }
    
    /**
     * Progress Condition: Wait-Free Population Oblivious on x86
     * <p>
     * Consistency Model: Sequentially Consistent with arrive() and isEmpty()
     */    
    @Override
    public void arrive() {
        UNSAFE.getAndAddLong(counters, byteOffset(tid2hash()), 1);
    }
    
    /**
     * Progress Condition: Wait-Free Population Oblivious on x86
     * <p>
     * Consistency Model: Sequentially Consistent with arrive() and isEmpty()
     */    
    @Override
    public void depart() {
        UNSAFE.getAndAddLong(counters, byteOffset(tid2hash()), -1);
    }
    
    /**
     * We read the first entry in the array with a volatile load, thus preventing 
     * regular loads from being re-ordered up, and in the end we do a loadFence()
     * to prevent regular loads from being re-ordered downwards.
     * <p>
     * Progress Condition: Wait-Free bounded by numCounters on x86
     * <p>
     * Consistency Model: Sequentially Consistent with increment() and decrement().
     */    
    @Override
    public boolean isEmpty() {
        long sum = aVolatileLoad;
        for (int idx = 0; idx < numCounters*CACHE_LINE; idx += CACHE_LINE) {
            sum += counters[idx]; 
        }
        UNSAFE.loadFence();
        return sum > 0;
    }
}
