package com.concurrencyfreaks.queues;

import java.lang.reflect.Field;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReferenceArray;

/**
 * <h1> Kogan-Petrank No Self-Linking Queue </h1>
 *
 * Based on the Wait-Free queue by Alex Kogan and Erez Petrank
 * https://offblast.org/stuff/books/lockfreequeues_ppopp11.pdf
 * http://www.cs.technion.ac.il/~erez/Papers/wfquque-ppopp.pdf
 * 
 * This is the same as the original code in the paper above, without self-linking of removed nodes
 *
 * enqueue algorithm: Kogan-Petrank, based on the consensus of Lamport's bakery
 * dequeue algorithm: Kogan-Petrank, based on the consensus of Lamport's bakery
 * Consistency: Linearizable
 * enqueue() progress: wait-free bounded O(N_threads)
 * dequeue() progress: wait-free bounded O(N_threads)
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia 
 */
public class KoganPetrankNoSLQueue<E> {
    
    private static class Node<E> {
        E value;
        volatile Node<E> next;
        int enqTid;
        AtomicInteger deqTid;

        public Node(E val, int etid) {
            value = val;
            next = null;
            enqTid = etid;
            deqTid = new AtomicInteger(-1);
        }

        public boolean casNext(Node<E> cmp, Node<E> val) {
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
                Class<?> k = Node.class;
                nextOffset = UNSAFE.objectFieldOffset
                    (k.getDeclaredField("next"));
            } catch (Exception e) {
                throw new Error(e);
            }
        }
    }

    
    private static class OpDesc<E> {
        final long phase;
        final boolean pending;
        final boolean enqueue;
        final Node<E> node;
        
        public OpDesc (long ph, boolean pend, boolean enq, Node<E> n) {
            phase = ph;
            pending = pend;
            enqueue = enq;
            node = n;
        }
    }
    
    private static final int NUM_THREADS = 128;
    
    // Member variables
    volatile Node<E> head;
    volatile Node<E> tail;
    final AtomicReferenceArray<OpDesc<E>> state;
    
    public KoganPetrankNoSLQueue() {
        final Node<E> sentinel = new Node<E>(null, -1);
        head = sentinel;
        tail = sentinel;
        state = new AtomicReferenceArray<OpDesc<E>>(NUM_THREADS);
        for (int i = 0; i < state.length(); i++) {
            state.set(i, new OpDesc<E>(-1, false, true, null));
        }
    }
    
    private void help(long phase)
    {
        for (int i = 0; i < state.length(); i++) {
            OpDesc<E> desc = state.get(i);
            if (desc.pending && desc.phase <= phase) {
                if (desc.enqueue) {
                    help_enq(i, phase);
                } else {
                    help_deq(i, phase);
                }
            }
        }
    }
    
    
    private long maxPhase() {
        long maxPhase = -1;
        for (int i = 0; i < state.length(); i++) {
            long phase = state.get(i).phase;
            if (phase > maxPhase) {
                maxPhase = phase;
            }
        }
        return maxPhase;
    }
     
    
    private boolean isStillPending(int tid, long ph) {
        return state.get(tid).pending && state.get(tid).phase <= ph;
    }
    
    
    public void enq(E value) {
        // We better have consecutive thread ids, otherwise this will blow up
        // TODO: replace this mechanism with something more flexible
        final int TID = (int)(Thread.currentThread().getId() % NUM_THREADS);
        long phase = maxPhase() + 1;
        state.set(TID, new OpDesc<E>(phase, true, true, new Node<E>(value, TID)));
        help(phase);
        help_finish_enq();
    }
    
    
    private void help_enq(int tid, long phase) {
        while (isStillPending(tid, phase)) {
            Node<E> last = tail;
            Node<E> next = last.next;
            if (last == tail) {           // If it's tail it can't be self-linked
                if (next == null) {
                    if (isStillPending(tid, phase)) {
                        if (last.casNext(next, state.get(tid).node)) {
                            help_finish_enq();
                            return;
                        }
                    }
                } else {
                    help_finish_enq();
                }
            }
        }
    }
    
    
    private void help_finish_enq() {
        final Node<E> last = tail;
        final Node<E> next = last.next;
        if (next != null) {
            int tid = next.enqTid;
            final OpDesc<E> curDesc = state.get(tid);
            if (last == tail && state.get(tid).node == next) {
                final OpDesc<E> newDesc = new OpDesc<E>(state.get(tid).phase, false, true, next);
                state.compareAndSet(tid, curDesc, newDesc);
                casTail(last, next);
            }
        }
    }
    
    
    public E deq() {
        // We better have consecutive thread ids, otherwise this will blow up
        // TODO: replace this mechanism with something more flexible
        final int TID = (int)(Thread.currentThread().getId() % NUM_THREADS);
        long phase = maxPhase() + 1;
        state.set(TID, new OpDesc<E>(phase, true, false, null));
        help(phase);
        help_finish_deq();
        final Node<E> node = state.get(TID).node;
        if (node == null) return null; // We return null instead of throwing an exception
        return node.next.value;
    }
    
    
    private void help_deq(int tid, long phase) {
        while (isStillPending(tid, phase)) {
            Node<E> first = head;
            Node<E> last = tail;
            Node<E> next = first.next;
            if (first == head) {  // If it's still head then it's not self-linked
                if (first == last) {
                    if (next == null) {
                        OpDesc<E> curDesc = state.get(tid);
                        if (last == tail && isStillPending(tid, phase)) {
                            OpDesc<E> newDesc = new OpDesc<E>(state.get(tid).phase, false, false, null);
                            state.compareAndSet(tid, curDesc, newDesc);
                        }
                    } else {
                        help_finish_enq();
                    }
                } else {
                    OpDesc<E> curDesc = state.get(tid);
                    Node<E> node = curDesc.node;
                    if (!isStillPending(tid, phase)) break;
                    if (first == head && node != first) {
                        OpDesc<E> newDesc = new OpDesc<E>(state.get(tid).phase, true, false, first);
                        if (!state.compareAndSet(tid, curDesc, newDesc)) {
                            continue;
                        }
                    }
                    first.deqTid.compareAndSet(-1, tid);
                    help_finish_deq();
                }
            }
        }
    }
    
    
    private void help_finish_deq() {
        final Node<E> first = head;
        final Node<E> next = first.next;
        int tid = first.deqTid.get();
        if (tid != -1 && next != first) {
            final OpDesc<E> curDesc = state.get(tid);
            if (first == head && next != null) {
                final OpDesc<E> newDesc = new OpDesc<E>(state.get(tid).phase, false, false, state.get(tid).node);
                state.compareAndSet(tid, curDesc, newDesc);
                casHead(first, next);
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
            Class<?> k = KoganPetrankNoSLQueue.class;
            tailOffset = UNSAFE.objectFieldOffset(k.getDeclaredField("tail"));
            headOffset = UNSAFE.objectFieldOffset(k.getDeclaredField("head"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }

}
