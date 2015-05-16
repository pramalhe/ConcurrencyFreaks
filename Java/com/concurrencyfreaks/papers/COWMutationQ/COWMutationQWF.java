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
package com.concurrencyfreaks.papers.COWMutationQ;

import java.lang.reflect.Field;
import java.util.concurrent.atomic.AtomicReferenceArray;


/**
 * <h1> Copy-On-Write with Mutation Queue (Wait-Free) wrapping a Collection C </h1>
 *
 * This is the same as COWMutationQLF but with a wait-free queue, and is 
 * therefore wait-free (bounded) for mutations.
 * This is a generic wrapper that can provide concurrent linearizable wait-free 
 * access (read or modify) to any object or data structure.
 * It's not particularly fast, just like the other known Copy-On-Write 
 * techniques, but it is the only technique we are aware of that can provide
 * generic linearizable wait-free (bounded) modifcations to any data structure,
 * even for mutating iterators.
 * 
 * <h2> Wait-Free Queue </h2>
 * The Wait-Free queue is an adaptation of the one by Alex Kogan and Alex Petrank:
 * https://offblast.org/stuff/books/lockfreequeues_ppopp11.pdf
 * Changes done to the wait-free queue:
 * <ul>
 * <li> Node has been embedded in MutationNode;
 * <li> We don't do dequeues so we removed all related methods;
 * <li> OpDesc.enqueue has been removed because we only do enqueues;
 * <li> Node.deqTid has been removed because it is only used by dequeue();
 * </ul>
 * 
 * <h2> The Copy-On-Write pattern </h2>
 * Up until now, there as been two known ways to apply the Copy-On-Write (COW) 
 * pattern to an object or data structure:
 * <ul>
 * <li> COW + Lock: A mutual exclusion lock is used to protect modify/write 
 *                  access to the object. This is Wait-Free Population 
 *                  Oblivious for read-only access, and Blocking for mutations.
 * <li> COW + CAS: A Compare-And-Swap operation is used to change the reference
 *                 providing access to the object. This is Lock-Free for mutations
 *                 and highly prone to starvation if the operations can take 
 *                 different times to complete.
 * <li> COW + MQ: The new technique used in this class. In this implementation
 *                we use a wait-free queue of mutations. Similarly to the other 
 *                COW techniques, this technique provides Wait-Free Population 
 *                Oblivious read-only access, but mutations (writes) are 
 *                wait-free (bounded).
 * </ul>              
 * We propose a new technique with a queue of mutations (COW + MQ) that can 
 * provide Wait-Free progress conditions for mutations. In this implementation 
 * we used Alex Kogan and Erez Petrank Wait-Free Queue algorithm which is 
 * wait-free for enqueing and dequeuing.
 * https://offblast.org/stuff/books/lockfreequeues_ppopp11.pdf
 * 
 * 
 * <h2> Algorithm components </h2>
 * The COW+MQ technique is composed of the following elements:
 * <ul>
 * <li> A queue of mutations where each node (MutationNode) stores a function 
 *      pointer (lambda) to be applied on the object (or data structure) and 
 *      the arguments with which the function should be called;
 * <li> A type that combines the head of the queue and the reference to the 
 *      object, named Combined;
 * <li> A single reference to the tail (last node) of the queue. Notice that
 *      there are multiple heads but only one tail;
 * <li> A reference to a Combined instance that contains the <i>current</i>
 *      head and object references;
 * </ul>
 * 
 * 
 * <h2> Algorithm description </h2>
 * The main steps of this technique for mutation (modify operations):
 * <ul>
 * <li> 1. Read the current Combined instance so we have a reference to the 
 *      object and to a corresponding MutationNode in the queue;
 * <li> 2. Create a new MutationNode with the desired mutation and insert
 *      it in the queue;
 * <li> 3. Clone the object whose referance was obtained from {@code combinedRef}
 *      and start applying to it the mutations in the queue, starting from head
 *      until reaching the node this thread inserted in the queue;
 * <li> 4. Do CAS on combinedRef from the current value up to a newly created
 *      Combined with this thread's MutationNode and the object with all the
 *      mutations up to it. Retry this if it fails until it succeeds or the 
 *      current instance of {@code combinedRef} has an {@code head} that is 
 *      after this thread's MutationNode in the queue;
 * </ul>
 * For read-only operations it's just a matter of reading the current 
 * Combined instance {@code combinedRef} and using its reference to the object. 
 * 
 * 
 * <h2> Async and Futures </h2>
 * This technique should <b>not</b> be confused with a technique that also uses 
 * a queue of mutations but process them asynchronously and returns the results 
 * of each mutation back (to the calling thread) as a future/promise.
 * Although such a technique can be implemented in a lock-free or even wait-free
 * way, it is <b>not</b> linearizable (or even sequentially consistent) and 
 * provides only serializabilty as its consistency model. The async+future 
 * technique does not provide an happens-before relationship between successive 
 * calls even for the same thread.
 * 
 * 
 * <h2> Non-obvious details </h2>
 * Although the CAS+MQ is a simple technique, there are a few non-obvious 
 * details which we now describe:
 * <ul>
 * <li> We need a Combined class to have an "atomic" way of accessing the 
 *      head and the reference to the object (or data structure). For
 *      the read-only methods, the reference to the object is enough, but
 *      for the mutable methods, we need to have a {@code head} and a
 *      {@code instance} so that we know which mutations need to be applied
 *      to that instance, until we reach the node containing the mutation
 *      that the current thread needs to execute.
 *      Notice that we could do this with a "double check pattern" where
 *      we would have a head and instance separated and do something like:
 *      check head -> check instance -> check that head hasn't changed
 *      The above procedure would be lock-free (but not wait-free) so we
 *      decided to implement the {@code Combined} class instead.
 *      
 * <li> The arguments to the mutation function are stored in the MutationNode
 *      and there are only three of them, but more can be added if so desired.
 *      The function for apply() will then need to be modified to accept more
 *      arguments as well. See QuadFunction.java
 *      
 * <li> We use a ShallowCopy interface to create the clone of the object (or
 *      data structure). We tried with Object.clone() but it's just not 
 *      possible. There might be a cleaner way to do this, but for the moment
 *      a practical implementation must implement ShallowCopy, which can be
 *      easily done with an inner class. For an example see
 *      CopyOnWriteMQLFTreeSet.CopyableTreeSet
 *      
 * <li> The queue starts by having the tail and the head (of the current 
 *      Combined) point to the same sentinel/dummy node, which contains no 
 *      mutation.
 *      
 * <li> The Combined is first read to obtain the head and only then do we 
 *      insert our MutationNode in the queue. This guarantees the invariant
 *      that the head of the Combined will always be before our own MutationNode.
 *      This detail is of vital important because on the last for() loop
 *      in applyMutation() we start from that head until we find our
 *      MutationNode or until we find the last node (ltail). 
 *      If we don't find our node in the list/queue, then it means the  
 *      current head is _after_ our node, which implies that the current
 *      combinedRef contains an instance that has our mutation already 
 *      applied to it. We can exit applyMutation() with the certainty that 
 *      our mutation is now visible to other threads.
 *      If we find our node, then it means that our mutation is not yet 
 *      visible, and we have to retry the CAS on combinedRef to make 
 *      our mutation (and the previous ones) visible to other threads.
 *           
 * <li> The applyMutation() method is wait-free (bounded), where the bound 
 *      typically depends on the number of simultaneous threads. The more
 *      threads there are attempting mutations, the more nodes will be added
 *      to the queue between reading the combinedRef and inserting our own 
 *      node, this creating a bound that in normal conditions should be
 *      not much larger than the number of threads, and thus explaining 
 *      why this technique is not Wait-Free Population Oblivious
 *      for mutations.
 *      
 * <li> A naive approach might consider doing the for() loop inside the 
 *      do/while up to null instead of a local tail (ltail), but this would 
 *      unfortunately be lock-free. The reason for it being lock-free is that
 *      another thread could add a new node to the queue, execute and do
 *      a succesful CAS on combinedRef, and then add a new node, and so on,
 *      while the current thread is traversing the queue, which would
 *      theoretically go on for ever (although highly unlikely in practice,
 *      to say the least).
 *      To provide wait-free guarantees, we read the current tail into 
 *      a local variable (ltail) after reading the current combinedRef and
 *      then do the for() loop up to ltail or myNode.
 * 
 * </ul>
 * 
 * 
 * <h2> How to use this class </h2>
 * See CopyOnWriteMQWFTreeSet.java on this same package.
 * 
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 *
 */
public class COWMutationQWF<P1, P2, P3, C extends ShallowCopy> {
    
    public static class MutationNode<P1, P2, P3, C> {
        final QuadFunction<P1, P2, P3, ? super C, ?> mutation;
        final P1 param1;
        final P2 param2;
        final P3 param3;
        volatile MutationNode<P1, P2, P3, C> next;   
        // Wait-free queue stuff:
        int enqTid;

        public MutationNode(QuadFunction<P1, P2, P3, ? super C, ?> mutationFunc, P1 p1, P2 p2, P3 p3, int etid) {
            this.mutation = mutationFunc;
            this.param1 = p1;
            this.param2 = p2;
            this.param3 = p3;
            this.next = null;
            this.enqTid = etid;
        }

        boolean casNext(MutationNode<P1, P2, P3, C> cmp, MutationNode<P1, P2, P3, C> val) {
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
                Class<?> k = MutationNode.class;
                nextOffset = UNSAFE.objectFieldOffset
                    (k.getDeclaredField("next"));
            } catch (Exception e) {
                throw new Error(e);
            }
        }
    }

    // class used by Wait-Free queue
    private static class OpDesc<P1, P2, P3, C> {
        final long phase;
        final boolean pending;
        final MutationNode<P1, P2, P3, C> node;
        
        public OpDesc (long ph, boolean pend, MutationNode<P1, P2, P3, C> n) {
            phase = ph;
            pending = pend;
            node = n;
        }
    }
    

    // Class to combine head and the instance    
    public static class Combined<P1, P2, P3, C> {
        final MutationNode<P1,P2,P3,C> head;
        public final C instance;
        public Combined(MutationNode<P1,P2,P3,C> head, C instance) {
            this.head = head;
            this.instance = instance;
        }
    }
    
    // TODO: change this mechanism, or at least increase this if you use more than 128 threads
    private static final int NUM_THREADS = 128;

    @sun.misc.Contended
    public volatile Combined<P1,P2,P3,C> combinedRef = null;
    
    // The tail of the queue/list of mutations.
    // Starts by pointing to a sentinel/dummy node
    @sun.misc.Contended
    private volatile MutationNode tail = new MutationNode<>(null, null, null, null, -1); 
    
    // Array of states used by the wait-free queue
    final AtomicReferenceArray<OpDesc<P1, P2, P3, C>> state;

    // Constructor with pre-existing instance
    public COWMutationQWF(C cloneableInstance) {
        // The head starts by pointing at the same node as tail
        combinedRef = new Combined(tail, cloneableInstance);
        state = new AtomicReferenceArray<OpDesc<P1,P2,P3,C>>(NUM_THREADS);
        for (int i = 0; i < state.length(); i++) {
            state.set(i, new OpDesc<P1,P2,P3,C>(-1, false, null));
        }

    }
    
    /**
     * 
     * Despite the multiple for() and while() loops, this method _is_ wait-free:
     * - enq() is wait-free (it's a wait-free queue).
     * - The first for() loop is wait-free because it will complete 
     *   in a finite number of steps: when it finds its own node.
     * - The do/while(!casRef(oldCombined, newCombined)) is wait-free because 
     *   the CAS will fail at most the number of nodes between the combinedRef
     *   with the original head and the combinedRef with the node previous to
     *   our own, i.e. a finite number of steps.
     * 
     * Progress Condition: Wait-Free (bounded)
     * 
     */
    public <R> R applyMutation(P1 param1, P2 param2, P3 param3, QuadFunction<P1, P2, P3, ? super C, R> mutativeFunc) {
        // We better have consecutive thread ids, otherwise this will blow up
        // TODO: replace this mechanism with something more flexible, like the CLQ+Array+Finalizers
        final int TID = (int)(Thread.currentThread().getId() % NUM_THREADS);

        final MutationNode<P1,P2,P3,C> myNode = new MutationNode<P1,P2,P3,C>(mutativeFunc, param1, param2, param3, TID);
        Combined<P1, P2, P3, C> curComb = combinedRef;
        // Insert our node in the queue
        enq(myNode, TID);
        
        // Clone the current instance and apply all mutations up until our node is reached
        final C mutatedInstance = (C)curComb.instance.copyOf();
        for (MutationNode<P1,P2,P3,C> mn = curComb.head.next; mn != myNode; mn = mn.next) {
            mn.mutation.apply(mn.param1, mn.param2, mn.param3, mutatedInstance);
        }
        // Save the return value of the last mutation (ours). We don't care about the other return values.
        final R retValue = mutativeFunc.apply(param1, param2, param3, mutatedInstance);
               
        // Create a new Combined with all the mutations up to ours (inclusive) 
        // and try to CAS the ref to it until it has our mutation.
        final Combined<P1,P2,P3,C> newComb = new Combined<P1,P2,P3,C>(myNode, mutatedInstance);
        do {
            if (curComb != combinedRef) {
                curComb = combinedRef;
                MutationNode<P1,P2,P3,C> ltail = tail;
                // Traverse the list until we reach the end, or our own node
                for (MutationNode<P1,P2,P3,C> mn = curComb.head; mn != myNode; mn = mn.next)
                    if (mn == ltail) return retValue; // Our mutation is visible
            }
        } while (!casRef(curComb, newComb));
        // Our mutation is now visible to other threads (through combinedRef)
        
        return retValue;
    }
        
    
    /*
     * Methods usd by the wait-free queue
     */
    
    private void help(long phase)
    {
        for (int i = 0; i < state.length(); i++) {
            OpDesc<P1,P2,P3,C> desc = state.get(i);
            if (desc.pending && desc.phase <= phase) {
                help_enq(i, phase);
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
    

    private void enq(MutationNode<P1,P2,P3,C> myNode, int TID) {
        long phase = maxPhase() + 1;
        state.set(TID, new OpDesc<P1,P2,P3,C>(phase, true, myNode));
        help(phase);
        help_finish_enq();
    }

    
    private void help_enq(int tid, long phase) {
        while (isStillPending(tid, phase)) {
            MutationNode<P1,P2,P3,C> last = tail;
            MutationNode<P1,P2,P3,C> next = last.next;
            if (last == tail) {
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
        final MutationNode<P1,P2,P3,C> last = tail;
        final MutationNode<P1,P2,P3,C> next = last.next;
        if (next != null) {
            int tid = next.enqTid;
            final OpDesc<P1,P2,P3,C> curDesc = state.get(tid);
            if (last == tail && state.get(tid).node == next) {
                final OpDesc<P1,P2,P3,C> newDesc = new OpDesc<P1,P2,P3,C>(state.get(tid).phase, false, next);
                state.compareAndSet(tid, curDesc, newDesc);
                casTail(last, next);
            }
        }
    }

    
    
    /*
     *  Unsafe mechanics
     */
    private static final sun.misc.Unsafe UNSAFE;
    private static final long tailOffset;
    private static final long refOffset;
    static {
        try {
            Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            UNSAFE = (sun.misc.Unsafe) f.get(null);
            Class<?> k = COWMutationQWF.class;
            tailOffset = UNSAFE.objectFieldOffset(k.getDeclaredField("tail"));
            refOffset = UNSAFE.objectFieldOffset(k.getDeclaredField("combinedRef"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }

    private boolean casTail(MutationNode<P1,P2,P3,C> cmp, MutationNode<P1,P2,P3,C> val) {
        return UNSAFE.compareAndSwapObject(this, tailOffset, cmp, val);
    }

    private boolean casRef(Combined<P1,P2,P3,C> cmp, Combined<P1,P2,P3,C> val) {
        return UNSAFE.compareAndSwapObject(this, refOffset, cmp, val);
    }    
}
