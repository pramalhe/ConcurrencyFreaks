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
package com.concurrencyfreaks.queues.array;

import java.lang.reflect.Field;
import java.util.concurrent.atomic.AtomicReferenceArray;

import com.concurrencyfreaks.queues.IQueue;


/**
 * <h1> Log 2 Array Queue </h1>
 *
 * Same as Linear Array Queue but does a binary (Log2) search on the array.
 *
 * This is a lock-free queue where each node contains an array of items.
 * Each entry in the array may contain on of three possible values:
 * - A valid item that has been enqueued;
 * - nullptr, which means no item has yet been enqueued in that position;
 * - taken, a special value that means there was an item but it has been dequeued;
 * The enqueue() searches for the first nullptr entry in the array and tries
 * to CAS from nullptr to its item.
 * The dequeue() searches for the first valid item in the array and tries to
 * CAS from item to "taken".
 * The search is done in a binary search which takes at most log2 steps.
 *
 * Enqueue algorithm: Log2 array search with CAS(nullptr,item)
 * Dequeue algorithm: Log2 array search with CAS(item,taken)
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Uncontended enqueue: 1 CAS 
 * Uncontended dequeue: 1 CAS 
 *
 * <p>
 * Possible improvements:
 * - We tried to make the indexing write on one cache line at a time
 *   but it didn't seem to make a difference. More research
 *  into this may be interesting;
 * - Create "nextEnq" and "nextDeq" atomic variables per node where each
 *   enqueuer and dequeuer write in the end of their operation the index
 *   of the next nullptr/item. This may be overwritten by an older thread
 *   but remember that this is per-thread so it may pay off.
 *   The store can be memory_order_release.
 *
 *
 * <p>
 * Lock-Free Linked List as described in Maged Michael and Michael Scott's paper:
 * {@link http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf}
 * <a href="http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf">
 * Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms</a>
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */

public class Log2ArrayQueue<E> implements IQueue<E> {

    static final int BUFFER_SIZE = 128;
    
    static class Node<E> {
        final AtomicReferenceArray<E> items = new AtomicReferenceArray<E>(BUFFER_SIZE);
        volatile Node<E> next = null;
        
        Node (final E item) {
            items.lazySet(0, item);
        }
        
        /**
         * @param cmp Previous {@code next}
         * @param val New {@code next}
         * @return {@code true} if CAS was successful
         */        
        boolean casNext(Node<E> cmp, Node<E> val) {
            return UNSAFE.compareAndSwapObject(this, nextOffset, cmp, val);
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
     
    @sun.misc.Contended
    private volatile Node<E> head;
    @sun.misc.Contended
    private volatile Node<E> tail;


    final E taken = (E)new Object(); // Muuuahahah !
    
    
    public Log2ArrayQueue() {
        final Node<E> startSentinel = new Node<E>(null);
        head = startSentinel;
        tail = startSentinel;
    }
    
    private int findFirstNull(Node<E> node) {
        if (node.items.get(0) == null) return 0;
        int minPos = 0;
        int maxPos = BUFFER_SIZE-1;
        while (true) {
            int pos = (maxPos-minPos)/2 + minPos;
            if (node.items.get(pos) == null) {
                maxPos = pos;
            } else {
                minPos = pos;
            }
            if (maxPos-minPos <= 3) return minPos;
        }
    }

    private int findLastTaken(Node<E> node) {
        if (node.items.get(BUFFER_SIZE-1) == taken) return BUFFER_SIZE-1;
        int minPos = 0;
        int maxPos = BUFFER_SIZE-1;
        while (true) {
            int pos = (maxPos-minPos)/2 + minPos;
            if (node.items.get(pos) == taken) {
                minPos = pos;
            } else {
                maxPos = pos;
            }
            if (maxPos-minPos <= 3) return minPos;
        }
    }
    
    
    
    /**
     * Progress Condition: Lock-Free
     * 
     * @param item must not be null
     */
    public void enqueue(E item) {
        if (item == null) throw new NullPointerException();
        while (true) {
            final Node<E> ltail = tail;
            if (ltail.items.get(BUFFER_SIZE-1) != null) { // This node is full
                if (ltail != tail) continue;
                final Node<E> lnext = ltail.next;
                if (lnext == null) {
                    final Node<E> newNode = new Node<E>(item);
                    if (ltail.casNext(null, newNode)) {
                        casTail(ltail, newNode);
                        return;
                    }
                } else {
                    casTail(ltail, lnext);
                }
                continue;
            }
            // Find the first null entry in items[] and try to CAS from null to item
            for (int i = findFirstNull(ltail); i < BUFFER_SIZE; i++) {
                if (ltail.items.get(i) != null) continue;
                if (ltail.items.compareAndSet(i, null, item)) return;
                if (ltail != tail) break;
            }
        }
    }
        
    /**
     * Progress condition: lock-free
     */
    public E dequeue() {
        Node<E> lhead = head;
        Node<E> node = lhead;
        while (node != null) {
            if (node.items.get(0) == null) return null;   // This node is empty
            if (node.items.get(BUFFER_SIZE-1) == taken) { // This node has been drained, check if there is another one
                node = node.next;
                continue;
            }            
            // Find the first non taken entry in items[] and try to CAS from item to taken
            for (int i = findLastTaken(node); i < BUFFER_SIZE; i++) {
                final E item = node.items.get(i);
                if (item == null) return null;            // This node is empty
                if (item == taken) continue;
                if (node.items.compareAndSet(i, item, taken)) {
                    if (node != lhead && head == lhead) casHead(lhead, node);
                    //lhead.next = lhead;                   // Do self-linking to help the GC
                    return item;
                }
                if (lhead != head) break;
            }            
        }
        return null;                                      // Queue is empty
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
            tailOffset = UNSAFE.objectFieldOffset(Log2ArrayQueue.class.getDeclaredField("tail"));
            headOffset = UNSAFE.objectFieldOffset(Log2ArrayQueue.class.getDeclaredField("head"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }
}
