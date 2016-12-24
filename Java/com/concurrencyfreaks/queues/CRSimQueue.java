/******************************************************************************
 * Copyright (c) 2016, Pedro Ramalhete, Andreia Correia
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
package com.concurrencyfreaks.queues;

import java.lang.reflect.Field;
import java.util.concurrent.atomic.AtomicIntegerArray;


/**
 * <h1> Correia-Ramalhete's variant of SimQueue </h1>
 * 
 * SimQueue is the original algorithm by Panagiota Fatourou and Nikolaos Kallimanis 
 * http://thalis.cs.uoi.gr/tech_reports/publications/TR2011-01.pdf
 * 
 * Our implementation is a simplified (and corrected) version of SimQueue.
 * 
 * Differences from the original SimQueue to CRSimQueue:
 * - We don't use Fetch-And-Add (FAA), but we could if we wanted to;
 * - There is no pointer_t or pools, we just allocate what we need on the fly;
 * - There are no HalfEnq or HalfDeq types;
 * - Our dequeue() uses the CRTurn consensus to be wait-free. The original SimQueue
 *   starts from zero which is a bit unfair but still wait-free.
 * 
 * Issues on the original SimQueue that have been fixed in CRSimQueue:
 * - There was a bug in dequeue() (line 307) that caused dequeue to return null almost
 *   always, except if it was running single-threaded;
 * - The shifts at the end of the while loop were done with (1L << pos) instead 
 *   of (1ULL << pos), thus resulting in undefined behavior in architectures 
 *   where long is 4 bytes. CRSimQueue doesn't do shifts and long in Java is 
 *   always 8 bytes so it's fine; 
 * - There was no check for consistency of re-used instances
 * 
 * 
 * Stuff that can be improved:
 * - We're using an array of integers for enqueuers[] when we should be using 
 *   an array of booleans;
 * - It's crappy that we need to allocate an heap array everytime we make a new 
 *   instance of EnqState and two arrays for DeqState, but I don't see another way;
 * - Padding is missing in enqueuers[], dequeuers[] and items[]. Not sure it is needed;
 * - No self-linking of dequeued nodes;
 * - We need 3 iterations in the main for() loop of enqueue() and not 2 because we don't
 *   have a connectQueue() method in dequeue(). We can call the 
 * 
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 *
 */
public class CRSimQueue<E> implements IQueue<E> {

    private final static int MAX_THREADS = 128;
    
    static class Node<E> {
        final E item;
        volatile Node<E> next;
        
        Node(final E item) {
            this.item  = item;
            this.next = null;
        }        
        
        /**
         * @param cmp Previous {@code next}
         * @param val New {@code next}
         * @return {@code true} if CAS was successful
         */        
        boolean casNext(Node<E> cmp, Node<E> val) {
            return UNSAFE.compareAndSwapObject(this, nextOffset, cmp, val);
        }
        
        void relaxedStoreNext(Node<E> newNext) {
            UNSAFE.putObject(this, nextOffset, newNext);
        }
        
        // Unsafe mechanics
        private static final sun.misc.Unsafe UNSAFE;
        private static final long nextOffset;

        static {
            try {
                Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
                f.setAccessible(true);
                UNSAFE = (sun.misc.Unsafe) f.get(null);
                nextOffset = UNSAFE.objectFieldOffset(Node.class.getDeclaredField("next"));
            } catch (Exception e) {
                throw new Error(e);
            }
        }        
    }
   
    
    // All the members in this class and their contents are immutable (afer visible to other threads)
    static class EnqState<E> {
        final Node<E> tail;       // The current tail
        final Node<E> nextNode;   // The next node to add to the tail (beginning of sublist)
        final Node<E> nextTail;   // The future tail, once tail.next becomes nextNode (end of sublist)
        final int[] applied;      // Enqueue requests to match enqueuers[]
        
        public EnqState(Node<E> ltail, Node<E> lnextNode, Node<E> lnextTail, int[] lapplied) {
            tail = ltail;
            nextNode = lnextNode;
            nextTail = lnextTail;
            applied = lapplied;
        }
    }

    // All the members in this class and their contents are immutable (afer visible to other threads)
    static class DeqState<E> {
        final Node<E> head;
        final int turn;
        final E[] items;
        final int[] applied;   
        
        public DeqState(Node<E> lhead, int lturn, E[] litems, int[] lapplied) {
            head = lhead;
            turn = lturn;
            items = litems;
            applied = lapplied;
        }
    }

    
    // Class variables
    private final int maxThreads;
    // Used by enqueuers
    private final AtomicIntegerArray enqueuers;
    private final E[] items;
    @sun.misc.Contended
    private volatile EnqState<E> enqState;
    // Used by dequeuers
    private final AtomicIntegerArray dequeuers;
    @sun.misc.Contended
    private volatile DeqState<E> deqState;

    
    
    public CRSimQueue() {
        this(MAX_THREADS);
    }
    

    public CRSimQueue(int maxThreads) {
        this.maxThreads = maxThreads;
        enqueuers = new AtomicIntegerArray(maxThreads);
        dequeuers = new AtomicIntegerArray(maxThreads);
        items = (E[])new Object[maxThreads];        
        final Node<E> sentinel = new Node<E>(null);
        enqState = new EnqState<E>(sentinel, null, sentinel, new int[maxThreads]);
        deqState = new DeqState<E>(sentinel, 0, (E[])new Object[maxThreads], new int[maxThreads]);
    }
    
    
    /**
     * Progress Condition: wait-free bounded
     * 
     * @param item must not be null
     */
    public boolean enqueue(E item) {
        enqueue(item, (int)(Thread.currentThread().getId() % maxThreads));
        return true;
    }
    
    public void enqueue(E item, final int tid) {
        if (item == null) throw new NullPointerException();
        // Publish enqueue request
        items[tid] = item;
        final int newrequest = (enqState.applied[tid]+1)%2;
        enqueuers.set(tid, newrequest);        
        for (int iter = 0; iter < 3; iter++) {
            final EnqState<E> lstate = enqState;
            // Advance the tail if needed
            if (lstate.tail.next != lstate.nextNode) lstate.tail.next = lstate.nextNode;
            // Check if my request has been done
            if (lstate.applied[tid] == newrequest) return;
            // Help other requests, starting from zero
            Node<E> first = null, node = null;
            int[] applied = lstate.applied.clone();
            for (int i = 0; i < maxThreads; i++) {
                // Check if it is an open request
                if (enqueuers.get(i) == applied[i]) continue;
                applied[i] = (applied[i]+1) % 2;
                Node<E> prev = node;
                node = new Node<E>(items[i]);
                if (first == null) {
                    first = node;
                } else {
                    prev.relaxedStoreNext(node);  // We don't want the volatile store here
                }
                if (lstate != enqState) break;
            }
            // Try to apply the new sublist
            if (lstate == enqState) casEnqState(lstate, new EnqState<E>(lstate.nextTail, first, node, applied));
        }
    }
      
    
    /**
     * Progress Condition: wait-free bounded
     */
    
    public E dequeue() {
        return dequeue((int)(Thread.currentThread().getId() % maxThreads));        
    }
    
    public E dequeue(final int tid) {
        DeqState<E> lstate = deqState;
        // Start by checking if the queue is empty
        if (lstate.head.next == null) return null;
        // Publish dequeue request
        final int newrequest = (lstate.applied[tid]+1) % 2;
        dequeuers.set(tid, newrequest);
        for (int iter = 0; iter < 2; iter++) {
            lstate = deqState;
            if (lstate.applied[tid] == newrequest) break;
            // Help opened dequeue requests, starting from turn+1
            Node<E> newHead = lstate.head;
            int newTurn = lstate.turn;
            E[] items = lstate.items.clone();
            int[] applied = lstate.applied.clone();
            Node<E> node = lstate.head;            
            for (int j = 1; j < maxThreads+1; j++) {
                final int i = (j + lstate.turn) % maxThreads;
                // Check if it is an open request
                if (dequeuers.get(i) == applied[i]) continue;
                applied[i] = (applied[i]+1) % 2;
                final Node<E> lnext = node.next;
                if (lnext == null) {
                    items[i] = null;
                } else {
                    node = lnext;
                    items[i] = node.item;
                    newHead = node;
                    newTurn = i;
                }
                if (lstate != deqState) break;
            }
            if (lstate != deqState) continue;
            if (casDeqState(lstate, new DeqState<E>(newHead, newTurn, items, applied))) break; 
        }
        return deqState.items[tid];
    }
    
        
    private boolean casEnqState(EnqState<E> cmp, EnqState<E> val) {
        return UNSAFE.compareAndSwapObject(this, enqStateOffset, cmp, val);
    }

    private boolean casDeqState(DeqState<E> cmp, DeqState<E> val) {
        return UNSAFE.compareAndSwapObject(this, deqStateOffset, cmp, val);
    }

    
    // Unsafe mechanics
    private static final sun.misc.Unsafe UNSAFE;
    private static final long enqStateOffset;
    private static final long deqStateOffset;
    static {
        try {
            Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            UNSAFE = (sun.misc.Unsafe) f.get(null);
            enqStateOffset = UNSAFE.objectFieldOffset(CRSimQueue.class.getDeclaredField("enqState"));
            deqStateOffset = UNSAFE.objectFieldOffset(CRSimQueue.class.getDeclaredField("deqState"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }
}
