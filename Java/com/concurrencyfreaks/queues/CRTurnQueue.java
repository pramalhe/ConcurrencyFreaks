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
 * <h1> CRTurn Queue </h1>
 * 
 * This is a Multi-Producer-Multi-Consumer Queue based on CRTurn for enqueuing and dequeuing
 * 
 * This implementation has self-linking
 * 
 * enqueue algorithm: CR Turn enqueue
 * dequeue algorithm: CR Turn dequeue
 * Consistency: Linearizable
 * enqueue() progress: wait-free bounded O(N_threads)
 * dequeue() progress: wait-free bounded O(N_threads)
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class CRTurnQueue<E> implements IQueue<E> {
    
    static class Node<E> {
        final E item;
        final int enqTid;
        volatile int deqTid;
        volatile Node<E> next;
        
        Node(final E item, final int tid) {
            this.item  = item;
            this.enqTid = tid;
            this.deqTid = IDX_NONE;
            this.next = null;
        }
        
        boolean casNext(Node<E> cmp, Node<E> val) {
            return UNSAFE.compareAndSwapObject(this, nextOffset, cmp, val);
        }
        
        boolean casDeqTid(int cmp, int val) {
            return UNSAFE.compareAndSwapInt(this, deqTidOffset, cmp, val);
        }

        // Unsafe mechanics
        private static final sun.misc.Unsafe UNSAFE;
        private static final long nextOffset;
        private static final long deqTidOffset;

        static {
            try {
                Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
                f.setAccessible(true);
                UNSAFE = (sun.misc.Unsafe) f.get(null);
                nextOffset = UNSAFE.objectFieldOffset(Node.class.getDeclaredField("next"));
                deqTidOffset = UNSAFE.objectFieldOffset(Node.class.getDeclaredField("deqTid"));
            } catch (Exception e) {
                throw new Error(e);
            }
        }        
    }
     
    
    private volatile Node<E> head;
    private volatile Node<E> tail;
    private final AtomicReferenceArray<Node<E>> enqueuers;
    private final AtomicReferenceArray<Node<E>> deqself;
    private final AtomicReferenceArray<Node<E>> deqhelp;

    private final static int IDX_NONE = -1;
    
    private final static int MAX_THREADS = 128;

    private final int maxThreads;
    
    public CRTurnQueue() {
        this(MAX_THREADS);
    }
    
    public CRTurnQueue(int maxThreads) {
        this.maxThreads = maxThreads;
        Node<E> sentinelNode = new Node<E>(null, getIndex());
        head = sentinelNode;
        tail = sentinelNode;
        enqueuers = new AtomicReferenceArray<Node<E>>(maxThreads);
        deqhelp = new AtomicReferenceArray<Node<E>>(maxThreads);
        deqself = new AtomicReferenceArray<Node<E>>(maxThreads);
        for (int i=0; i < maxThreads; i++) {
            // deqself[i] != deqhelp[i] means that isRequest=false
            deqself.lazySet(i, sentinelNode);
            deqhelp.lazySet(i, new Node<E>(null, 0));
        }        
    }
    
    private int getIndex() {
        return (int)(Thread.currentThread().getId() % maxThreads);
    }
    
    
    /**
     * The single-producer order is like this:
     * 1. Add node to enqueuers[]
     * 2. Insert node in tail.next using a CAS
     * 3. Advance tail to tail.next
     * 4. Remove node from enqueuers[]
     *  
     * Progress Condition: Wait-Free Bounded by the number of threads calling enqueue()
     * @param item must not be null
     */
    public void enqueue(E item) {
        if (item == null) throw new NullPointerException();
        final int myidx = getIndex();
        final Node<E> myNode = new Node<E>(item, myidx);
        enqueuers.set(myidx, myNode);                    // Do step 1: add node to enqueuers[]
        for (int i = 0; i < maxThreads; i++) {
            Node<E> ltail = tail;
            if (enqueuers.get(ltail.enqTid) == ltail) {  // Help a thread do step 4
                enqueuers.compareAndSet(ltail.enqTid, ltail, null);
            }
            if (enqueuers.get(myidx) == null) return;    // Some thread helped me and did all the work, yupiii! (INV3)
            for (int j = 1; j < maxThreads+1; j++) {     // Help a thread do step 2
                Node<E> nodeToHelp = enqueuers.get((j + ltail.enqTid) % maxThreads);
                if (nodeToHelp == null) continue;
                ltail.casNext(null, nodeToHelp);
                break;
            }
            Node<E> lnext = ltail.next;                  // Help a thread do step 3
            if (lnext != null) casTail(ltail, lnext);
        }
        enqueuers.lazySet(myidx, null);                  // Do step 4, just in case it's not done 
    }
    
    
    
    /**
     * @param idx_deq, usualy is -1
     */
    private int searchNext(Node<E> lhead, Node<E> lnext) {
        final int turn = lhead.deqTid;
        for (int idx=turn+1; idx < turn+maxThreads+1; idx++) {
            final int idDeq = idx%maxThreads;
            // It's a request if the two entries are the same.
            if (deqself.get(idDeq) != deqhelp.get(idDeq)) continue;
            if (lnext.deqTid == IDX_NONE) lnext.casDeqTid(IDX_NONE, idDeq);
            break;
        }
        return lnext.deqTid;
    }
    
    
    private void casDeqAndCasHead(Node<E> lhead, Node<E> lnext, int myidx) {
        final int ldeqTid = lnext.deqTid;
        if (ldeqTid == myidx) {
            deqhelp.lazySet(ldeqTid, lnext);
        } else {
            final Node<E> ldeqhelp = deqhelp.get(ldeqTid);
            if (ldeqhelp != lnext && lhead == head) {
                deqhelp.compareAndSet(ldeqTid, ldeqhelp, lnext); // Assign next to request
            }
        }
        casHead(lhead, lnext);
        lhead.next = lhead;     // Self-linking to help the GC
    }
    
    
    private void giveUp(Node<E> req, int myidx) {
        Node<E> lhead = head;
        if (deqhelp.get(myidx) != req) return;
        if (lhead == tail) return;        
        Node<E> lnext = lhead.next;
        if (lhead != head) return;
        // make sure Node next is assign to a request for dequeue
        if (searchNext(lhead, lnext) == IDX_NONE) {
            lnext.casDeqTid(IDX_NONE, myidx);
        }
        casDeqAndCasHead(lhead, lnext, myidx);
    }

    
    /**
     * The single consumer order is:
     * 1. Add request to dequeuers[myidx]
     * 2. Assign ownership of next node on the list to self using head.next.casIdx(IDX_REQUEST, myidx)
     * 3. Assign node with ownership to dequeurs[myidx] using a CAS(request,node)
     * 4. Advance head
     * 5. Unpublish by doing a CAS on dequeuers[myidx], CAS(node,null) 
     */
    public E dequeue() {    	
    	final int myidx = (int)(Thread.currentThread().getId() % maxThreads);
        final Node<E> prReq = deqself.get(myidx);
        final Node<E> myReq = deqhelp.get(myidx);
        deqself.set(myidx, myReq);             // isRequest=true
        for (int i=0; i < maxThreads; i++) {
            if (deqhelp.get(myidx) != myReq) break;
            Node<E> lhead = head;
            if (lhead == tail) {           // Give up
                deqself.set(myidx,prReq);  // isRequest=false
                giveUp(myReq, myidx);
                if (deqhelp.get(myidx) != myReq) {
                    deqself.set(myidx, myReq);
                    break;
                }
                return null;
            }
            Node<E> lnext = lhead.next;
            if (lhead != head) continue;
            if (searchNext(lhead, lnext) != IDX_NONE) casDeqAndCasHead(lhead, lnext, myidx);
        }
        Node<E> myNode = deqhelp.get(myidx);   
        Node<E> lhead = head;
        // Check if step 4 is needed for my node.
        if (lhead == head && myNode == lhead.next) {
            casHead(lhead, myNode);
            lhead.next = lhead;      // Self-linking to help the GC
        }
        return myNode.item;
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
            tailOffset = UNSAFE.objectFieldOffset(CRTurnQueue.class.getDeclaredField("tail"));
            headOffset = UNSAFE.objectFieldOffset(CRTurnQueue.class.getDeclaredField("head"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }  
}
