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
import java.util.concurrent.atomic.AtomicReferenceArray;



/**
 * <h1> The Encapsulator Queue </h1>
 * 
 * This is a Multi-Producer-Multi-Consumer Queue that aggregates enqueues.
 * It is wait-free for enqueues and lock-free for dequeues.
 * Each node has an array of items that were attempting to be enqueued at
 * a given time.
 * Dequeueing is done by doing a CAS on Encap.item from the current item's
 * value to null. If the Encap.item is already null, skip over to the next
 * item, or return null if there is no node left.
 * 
 * 
 * enqueue algorithm: Encapsulator algorithm
 * dequeue algorithm: Does a CAS in each Encap until it succeeds
 * Consistency: Linearizable
 * enqueue() progress: wait-free bounded O(N_threads)
 * dequeue() progress: lock-free
 * 
 * This queue does self-linking to help the GC.
 * 
 * Possible improvements include:
 * - Instead of always allocating a new "lreqs", pre-allocate it, one per thread;
 * - In dequeue(), we read the Encap instance from node.encaps[(tid + i) % length] 
 *   which means we do it on a different order for each thread. Wether this is 
 *   really better for throughput/contention still remains to be seen;
 * - Check if keeping the original (larger) array is preferable to making a new 
 *   array and copying it, which is what we now do in the constructor of Node();
 *   
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class EncapsulatorQueue<E> implements IQueue<E> {
    
    static class Node<E> {
        final Encap<E>[] encaps;
        volatile Node<E> next = null;
        
        public Node(Encap<E>[] reqs, int used) {
            encaps = new Encap[used];
            // Make a copy of the array so we don't have to carry "length" around
            System.arraycopy(reqs, 0, encaps, 0, used); 
        }
                
        boolean casNext(Node<E> cmp, Node<E> val) {
            return UNSAFE.compareAndSwapObject(this, nextOffset, cmp, val);
        }

        void putOrderedNext(Node<E> val) {
            UNSAFE.putOrderedObject(this, nextOffset, val);
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

    // Encapsulates an item. Set to null when the item has been dequeued
    static class Encap<E> {
        volatile E item;

        public Encap() { }

        public Encap(E item) { this.item = item; }
        
        boolean casItem(E cmp, E val) {
            return UNSAFE.compareAndSwapObject(this, itemOffset, cmp, val);
        }
        // Unsafe mechanics
        private static final sun.misc.Unsafe UNSAFE;
        private static final long itemOffset;

        static {
            try {
                Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
                f.setAccessible(true);
                UNSAFE = (sun.misc.Unsafe) f.get(null);
                itemOffset = UNSAFE.objectFieldOffset(Encap.class.getDeclaredField("item"));
            } catch (Exception e) {
                throw new Error(e);
            }
        }        
    }
    
    private volatile Node<E> head;
    private volatile Node<E> tail;
    private final AtomicReferenceArray<Encap<E>> enqueuers;
    
    private final static int MAX_THREADS = 128;

    private final int maxThreads;
    
    public EncapsulatorQueue() {
        this(MAX_THREADS);
    }
    
    public EncapsulatorQueue(int maxThreads) {
        this.maxThreads = maxThreads;
        Node<E> sentinelNode = new Node<E>(new Encap[0],0);
        head = sentinelNode;
        tail = sentinelNode;
        enqueuers = new AtomicReferenceArray<Encap<E>>(maxThreads);
    }
    
    
    public void enqueue(E item) {
        enqueue(item, (int)(Thread.currentThread().getId() % maxThreads));
    }
    
    /**
     * Progress Condition: wait-free
     * 
     * @param item must not be null
     * @param tid must be a UNIQUE thread id in the range 0 to maxThreads-1
     */
    public void enqueue(E item, final int tid) {
        if (item == null) throw new NullPointerException();
        final Encap<E> myEncap = new Encap<E>(item);
        enqueuers.set(tid, myEncap);  // Open request
        final Encap<E>[] lreqs = new Encap[maxThreads]; 
        for (int iter = 0; iter < 2; iter++) {
            Node<E> ltail = tail;
            if (ltail.next != null) { // Advance tail if needed
                casTail(ltail, ltail.next);
                ltail = tail;
                if (ltail.next != null) continue;
            }
            int numreqs = 0;
            for (int i = 0; i < maxThreads; i++) {
                final Encap<E> encap = enqueuers.get(i);
                if (encap == null) continue;
                lreqs[numreqs++] = encap;
            }
            if (ltail != tail || ltail.next != null) continue;
            if (ltail.casNext(null, new Node<E>(lreqs, numreqs))) {
                casTail(ltail, ltail.next);
                break;
            }
        }
        enqueuers.lazySet(tid, null);
    }
    
    
    public E dequeue() {
        return dequeue((int)(Thread.currentThread().getId() % maxThreads));
    }
    
    /**
     * Progress Condition: lock-free
     * 
     * @param tid Can be anything, we don't use it for the dequeue()
     * @return Returns the dequeued item
     */
    public E dequeue(final int tid) {
        Node<E> lhead = head;
        Node<E> node = lhead;
        while (node != null) {
            if (node.next == node) { // Handle self-linking
                lhead = head;
                node = lhead;
            }
            final int length = node.encaps.length;
            for (int i = 0; i < length; i++) {
                final Encap<E> encap = node.encaps[(tid + i) % length];
                final E item = encap.item;
                if (item == null) continue;
                if (encap.casItem(item, null)) {
                    if (node != lhead && lhead == head) {
                        if (casHead(lhead, node)) {
                            // Self-link to help the GC
                            if (lhead != tail) lhead.putOrderedNext(lhead); 
                        }
                    }
                    return item;
                }
            }
            node = node.next;
        }
        return null;
    }

    
    private boolean casTail(Node<E> cmp, Node<E> val) {
        return UNSAFE.compareAndSwapObject(this, tailOffset, cmp, val);
    }

    private boolean casHead(Node<E> cmp, Node<E> val) {
        return UNSAFE.compareAndSwapObject(this, headOffset, cmp, val);
    }

    // Unsafe mechanics
    private static final sun.misc.Unsafe UNSAFE;
    private static final long tailOffset;
    private static final long headOffset;
    static {
        try {
            Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            UNSAFE = (sun.misc.Unsafe) f.get(null);
            tailOffset = UNSAFE.objectFieldOffset(EncapsulatorQueue.class.getDeclaredField("tail"));
            headOffset = UNSAFE.objectFieldOffset(EncapsulatorQueue.class.getDeclaredField("head"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }  
}
