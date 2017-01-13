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


/**
 * <h1> Double Linked Queue </h1>
 *
 * Double Linked Queue based on the short paper by Andreia Correia and Pedro Ramalhete
 * https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/doublelink-2016.pdf
 * 
 * <br> enqueue algorithm: Double-linked enqueue with relaxed store
 * <br> dequeue algorithm: Michael-Scott + help link last enqueue
 * <br> Consistency: Linearizable
 * <br> enqueue() progress: lock-free
 * <br> dequeue() progress: lock-free
 * <br> Uncontended enqueue: 1 CAS
 * <br> Uncontended dequeue: 1 CAS
 * <p>
 * When running uncontended, this algorithm does one CAS and one store to enqueue
 * and one CAS to dequeue, thus improving over Michael-Scott which does two CAS
 * to enqueue (and one to dequeue). As a result, this queue is almost 2x faster
 * at doing uncontended enqueues than the Michael-Scott queue.
 * The dequeue mechanism is pretty much the one by Michael-Scott (minor modifications).
 * http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf
 * <p>
 * We do self-linking, but on a best-effort approach. We don't guarantee that
 * all the nodes will be self-linked, particularly the {@code node.prev}.
 * <p>
 * <p>
 * About the (non)-usage of volatiles:
 * We don't need volatile in "next" because: 
 * <br> In enqueue() we synchronize on the "tail". This means that newly 
 * inserted nodes will become visible to the other enqueuers if the tail 
 * advanced, seen that "tail" is a volatile.
 * <br> In dequeue() we synchronize also on the "tail" because it's the
 * first thing we check to see if the queue is empty or not. All nodes
 * up to tail will have their "prev" visible, with at most the node before
 * the current "tail" not having its next visible (as dictated by the 
 * DoubleLink algorithm), in which case we will write race in:
 * if (lprev.next == null && lprev != ltail) lprev.next = ltail; 
 * 
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 *
 */
public class CRDoubleLinkQueue<E> implements IQueue<E> {

    static class Node<E> {
        final E item;
        Node<E> next;  // yep, no volatile
        Node<E> prev;  // yep, no volatile
        
        Node(final E item) {
            this.item  = item;
            this.next = null;
            this.prev = null;
        }                
    }
     
    @sun.misc.Contended
    private volatile Node<E> head;
    @sun.misc.Contended
    private volatile Node<E> tail;

    
    
    public CRDoubleLinkQueue() {
        final Node<E> prevSentinel = new Node<E>(null);
        final Node<E> startSentinel = new Node<E>(null);
        startSentinel.prev = prevSentinel;
        prevSentinel.next = startSentinel;
        head = startSentinel;
        tail = startSentinel;
    }
    
    
    /**
     * Progress Condition: Lock-Free
     * 
     * Implementation detail about self-linking:
     * When using a GC (like in Java), there is a rare case where setting the 
     * 'next' will undo a self-link, and an even rarer case where this occurs
     * for contiguous nodes. The GC should take care of it anyways, the 
     * self-linking is not needed for correctness, we do it to help the GC.
     * 
     * @param item must not be null
     */
    public boolean enqueue(E item) {
        if (item == null) throw new NullPointerException();
        final Node<E> newNode = new Node<E>(item);
        while (true) {
            final Node<E> ltail = tail;
            final Node<E> lprev = ltail.prev;
            // Help the previous enqueue() to complete
            if (lprev.next == null && lprev != ltail) lprev.next = ltail;          
            newNode.prev = ltail;
            if (casTail(ltail, newNode)) {
                ltail.next = newNode;        // This can be relaxed because the dequeuer helps to link the tail
                return true;
            }
        }
    }
        
    
    /**
     * Progress Condition: Lock-Free
     * 
     * Unlike the C++ version, in the Java version we help the enqueue() to 
     * complete by linking the tail. This makes the code a bit larger, but 
     * we don't need volatile in "next", which improves throughput.
     */
    public E dequeue() {  
        while (true) {
            final Node<E> lhead = head;
            // Help the previous enqueue() to complete
            final Node<E> ltail = tail;
            final Node<E> lprev = ltail.prev;
            if (lprev.next == null && lprev != ltail) lprev.next = ltail;
            final Node<E> lnext = lhead.next; 
            if (lhead == ltail || lnext == null) return null;  // Queue is empty
            if (lnext == lhead) continue;    // Re-read head if it's self-linked
            if (casHead(lhead, lnext)) {
                lnext.prev = lnext;          // Do self-linking to help the GC. This is a relaxed store
                lhead.next = lhead;          // Can be a relaxed store
                return lnext.item;
            }
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
            tailOffset = UNSAFE.objectFieldOffset(CRDoubleLinkQueue.class.getDeclaredField("tail"));
            headOffset = UNSAFE.objectFieldOffset(CRDoubleLinkQueue.class.getDeclaredField("head"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }
}
