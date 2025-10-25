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
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReferenceArray;

import com.concurrencyfreaks.queues.IQueue;


/**
 * <h1> Fetch-And-Add Array Queue </h1>
 *
 * Each node has one array but we don't search for a vacant entry. Instead, we
 * use FAA to obtain an index in the array, for enqueueing or dequeuing.
 *
 * There are some similarities between this queue and the basic queue in YMC:
 * http://chaoran.me/assets/pdf/wfq-ppopp16.pdf
 * but it's not the same because the queue in listing 1 is obstruction-free, while
 * our algorithm is lock-free.
 * In FAAArrayQueue eventually a new node will be inserted (using Michael-Scott's
 * algorithm) and it will have an item pre-filled in the first position, which means
 * that at most, after BUFFER_SIZE steps, one item will be enqueued (and it can then
 * be dequeued). This kind of progress is lock-free.
 *
 * Each entry in the array may contain one of three possible values:
 * - A valid item that has been enqueued;
 * - nullptr, which means no item has yet been enqueued in that position;
 * - taken, a special value that means there was an item but it has been dequeued;
 *
 * Enqueue algorithm: FAA + CAS(null,item)
 * Dequeue algorithm: FAA + CAS(item,taken)
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Pointers (lock-free)
 * Uncontended enqueue: 1 FAA + 1 CAS + 1 HP
 * Uncontended dequeue: 1 FAA + 1 CAS + 1 HP
 *
 *
 * <p>
 * Lock-Free Linked List as described in Maged Michael and Michael Scott's paper:
 * {@link http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf}
 * <a href="http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf">
 * Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms</a>
 * <p>
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * https://web.archive.org/web/20140216055028/http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class FAAArrayQueue<E> implements IQueue<E> {

    static final int BUFFER_SIZE = 128;
    
    static class Node<E> {
        final AtomicInteger deqidx = new AtomicInteger(0);
        final AtomicReferenceArray<E> items = new AtomicReferenceArray<E>(BUFFER_SIZE);
        final AtomicInteger enqidx = new AtomicInteger(1);
        volatile Node<E> next = null;
        // Start with the first entry pre-filled and enqidx at 1        
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
    
    
    public FAAArrayQueue() {
        final Node<E> sentinelNode = new Node<E>(null);
        sentinelNode.enqidx.set(0);
        head = sentinelNode;
        tail = sentinelNode;
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
            final int idx = ltail.enqidx.getAndIncrement();
            if (idx > BUFFER_SIZE-1) { // This node is full
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
            if (ltail.items.compareAndSet(idx, null, item)) return;
        }
    }
    
        
    /**
     * Progress condition: lock-free
     */
    public E dequeue() {
        while (true) {
            Node<E> lhead = head;
            if (lhead.deqidx.get() >= lhead.enqidx.get() && lhead.next == null) return null;
            final int idx = lhead.deqidx.getAndIncrement();
            if (idx > BUFFER_SIZE-1) { // This node has been drained, check if there is another one
                if (lhead.next == null) return null;  // No more nodes in the queue
                casHead(lhead, lhead.next);
                continue;
            }
            final E item = lhead.items.getAndSet(idx, taken); // We can use a CAS instead
            if (item != null) return item; 
        }
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
            tailOffset = UNSAFE.objectFieldOffset(FAAArrayQueue.class.getDeclaredField("tail"));
            headOffset = UNSAFE.objectFieldOffset(FAAArrayQueue.class.getDeclaredField("head"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }
}
