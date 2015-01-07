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
package com.concurrencyfreaks.readindicators;

import java.util.concurrent.ConcurrentLinkedQueue;


/**
 * <h1> Read Indicator with ConcurrentLinkedQueue of Thread references </h1>
 * 
 * <ul>
 * <li> {@link arrive()}  - O(1), Lock-Free
 * <li> depart()  - O(N_Threads), Lock-Free
 * <li> isEmpty() - O(1), Lock-Free
 * </ul>
 * Memory usage: The number of nodes is equal to the number of threads currently "arrived".
 * No threads "arrived" means no memory usage, apart from the empty CLQ instance.
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class RIThreadCLQ implements ReadIndicator {
    private final ConcurrentLinkedQueue<Thread> clq = new ConcurrentLinkedQueue<Thread>(); 
   
    @Override
    public void arrive() {
        clq.add(Thread.currentThread());
    }
    
    @Override
    public void depart() {
        clq.remove(Thread.currentThread());
    }
    
    @Override
    public boolean isEmpty() {
        return clq.isEmpty();
    }
}
