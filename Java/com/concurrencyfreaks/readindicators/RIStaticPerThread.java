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

import java.util.concurrent.atomic.AtomicLongArray;

public class RIStaticPerThread implements ReadIndicator {
    private final AtomicLongArray perThreadState;
    private final int maxNumThreads;
    
    private static final int STATE_NOT_READING = 0;
    private static final int STATE_READING = 1;
    
    public RIStaticPerThread(int maxNumThreads) {
        this.maxNumThreads = maxNumThreads;
        this.perThreadState = new AtomicLongArray(maxNumThreads);
    }

    @Override
    public void arrive() {
        perThreadState.set((int)Thread.currentThread().getId(), STATE_READING);
    }
    
    @Override
    public void depart() {
        perThreadState.set((int)Thread.currentThread().getId(), STATE_NOT_READING);
    }
    
    @Override
    public boolean isEmpty() {
        for (int tid = 0; tid < maxNumThreads; tid++) {
            if (perThreadState.get(tid) == STATE_READING) return false;
        }
        return true;
    }
}
