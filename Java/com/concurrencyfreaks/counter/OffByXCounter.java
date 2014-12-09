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
 * A Counter that uses the "Off-by-X" pattern.
 * On x86 this is probably not much faster, but on other architectures 
 * where re-ordering is possible (like ARMv8), this should give much better
 * performance.
 * <p>
 * Performance plots and a longer description can be seen on this post:
 * <a href="http://concurrencyfreaks.com/2013/08/concurrency-pattern-off-by-x-counter.html">Off-by-X Counter</a>
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class OffByXCounter {
    
    // Member variables
    private long counter = 0;
    private long counterVolatile = 0;
    private final long bitflag;
    
    
    // For faster calls to increment(), the 'theX' variable should be a power of 2
    public OffByXCounter(int theX) {
        this.bitflag = Integer.highestOneBit(theX) - 1;
    }
    
    /**
     * Do a volatile load of 'counter'.
     * Can be called from multiple threads simultaneously (i.e. thread-safe)
     * @return
     */
    public long get() {
        return counterVolatile;
    }
    
    /** 
     * We use a volatile that is only updated once in 'theX' times and regular 
     * variable, this way we guarantee atomicity and monotonic progress for 
     * the 'counter' variable.
     * This method can NOT be called simultaneously from multiple threads. 
     * 
     * We tried doing a non-volatile store in counterVolatile for every time
     * that (counter & bitflag) != 0 but it would kill performance.
     */ 
    public void increment() {
        counter++;
        if ((counter & bitflag) == 0) counterVolatile = counter;
    }

    /**
     * Resets the counter.
     * This method can NOT be called simultaneously from multiple threads.
     */
    public void clear() {
        counter = 0;
        counterVolatile = 0;
    }
}
