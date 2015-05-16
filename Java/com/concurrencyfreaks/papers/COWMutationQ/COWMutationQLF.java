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

/**
 * <h1> Copy-On-Write with Mutation Queue (Lock-Free) wrapping a Collection C </h1>
 * 
 * This is a generic wrapper that can provide concurrent linearizable lock-free
 * mutative access and wait-free read-only access to any object or data structure.
 * It's not particularly fast or scalable, just like the other known Copy-On-Write 
 * techniques, but it can provide better throughput than the other COW techniques
 * unless a small data structure or object is used.  
 * 
 * 
 * <h2> The Copy-On-Write pattern </h2>
 * Up until now, there as been two known ways to apply the Copy-On-Write (COW) 
 * pattern to an object or data structure:
 * <ul>
 * <li> COWLock: A mutual exclusion lock is used to protect modify/write 
 *               access to the object. This is Wait-Free Population 
 *               Oblivious for read-only access, and Blocking for mutations.
 * <li> COWCAS:  A Compare-And-Swap operation is used to change the reference
 *               providing access to the object. This is Lock-Free for mutations
 *               and highly prone to starvation if the operations can take 
 *               different times to complete.
 * <li> COWMQ:   The new technique used in this class. In this implementation
 *               we use a lock-free queue of mutations but it is otherwise
 *               wait-free. Similarly to the other COW techniques, this 
 *               technique provides Wait-Free Population Oblivious read-only
 *               access, but mutations (writes) are lock-free, or event
 *               wait-free (bounded) if a wait-free queue is used instead.
 * </ul>              
 * We propose a new technique with a queue of mutations (COWMQ) that can 
 * provide (not easily starvable) Lock-Free progress conditions for mutations
 * if a Lock-Free queue is used, or even Wait-Free (bounded) if a wait-free
 * queue is used. In this implementation we used M. Michael and M. Scott's
 * queue algorithm which is lock-free for all operations.
 * 
 * 
 * <h2> Algorithm components </h2>
 * The COWMQ technique is composed of the following elements:
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
 * Although the CASMQ is a simple technique, there are a few non-obvious 
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
 * </ul>
 * 
 * 
 * <h2> How to use this class </h2>
 * See CopyOnWriteMQLFTreeSet.java on this same package. 
 * 
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 *
 */
public class COWMutationQLF<P1, P2, P3, C extends ShallowCopy> {
    
    public static class MutationNode<P1, P2, P3, C> {
        final QuadFunction<P1, P2, P3, ? super C, ?> mutation;
        final P1 param1;
        final P2 param2;
        final P3 param3;
        volatile MutationNode<P1, P2, P3, C> next;   
        
        public MutationNode(P1 p1, P2 p2, P3 p3, QuadFunction<P1, P2, P3, ? super C, ?> mutativeFunc) {
            this.mutation = mutativeFunc;
            this.param1 = p1;
            this.param2 = p2;
            this.param3 = p3;
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
    
    // Class to combine head and the instance
    public static class Combined<P1, P2, P3, C> {
        final MutationNode<P1,P2,P3,C> head;
        public final C instance;
        public Combined(MutationNode<P1,P2,P3,C> head, C instance) {
            this.head = head;
            this.instance = instance;
        }
    }

    @sun.misc.Contended
    public volatile Combined<P1,P2,P3,C> combinedRef = null;
    
    // The tail of the queue/list of mutations.
    // Starts by pointing to a dummy node
    @sun.misc.Contended
    private volatile MutationNode tail = new MutationNode<>(null, null, null, null); 
    

    // Constructor with pre-existing instance
    public COWMutationQLF(C cloneableInstance) {
        // The head starts by pointing at the same node as tail
        combinedRef = new Combined(tail, cloneableInstance);
    }
    
    /**
     * 
     * Despite the multiple for() and while() loops, the only part of this method that is 
     * not Wait-Free (Bounded) is the call to addToTail() which is Lock-Free.
     * 
     * Progress Condition: Lock-Free
     * 
     */
    public <R> R applyMutation(P1 param1, P2 param2, P3 param3, QuadFunction<P1, P2, P3, ? super C, R> mutativeFunc) {
        final MutationNode<P1,P2,P3,C> myNode = new MutationNode<P1,P2,P3,C>(param1, param2, param3, mutativeFunc);
        Combined<P1,P2,P3,C> curComb = combinedRef;
        // Insert our node in the queue
        addToTail(myNode);
        
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
    
    
    /**
     * Adds 'myNode' to the tail (last node) of the queue/linked list
     * Uses Michael and Scott's algorithm, same as ConcurrentLinkedQueue
     * 
     * Progress Condition: Lock-Free
     * 
     * @param mynode
     */
    private void addToTail(final MutationNode<P1,P2,P3,C> myNode) {
        while (true) {
            final MutationNode<P1,P2,P3,C> localTail = tail;
            final MutationNode<P1,P2,P3,C> node = localTail.next;
            if (localTail == tail) {
                if (node == null) {
                    // localTail is last node
                    if (localTail.casNext(null, myNode)) {
                        casTail(localTail, myNode);
                        return;
                    }
                } else {
                    casTail(localTail, node);
                }
            }
        }
    }
    
    private boolean casTail(MutationNode<P1,P2,P3,C> cmp, MutationNode<P1,P2,P3,C> val) {
        return UNSAFE.compareAndSwapObject(this, tailOffset, cmp, val);
    }

    private boolean casRef(Combined<P1,P2,P3,C> cmp, Combined<P1,P2,P3,C> val) {
        return UNSAFE.compareAndSwapObject(this, refOffset, cmp, val);
    }
    
    // Unsafe mechanics
    private static final sun.misc.Unsafe UNSAFE;
    private static final long tailOffset;
    private static final long refOffset;
    static {
        try {
            Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            UNSAFE = (sun.misc.Unsafe) f.get(null);
            Class<?> k = COWMutationQLF.class;
            tailOffset = UNSAFE.objectFieldOffset(k.getDeclaredField("tail"));
            refOffset = UNSAFE.objectFieldOffset(k.getDeclaredField("combinedRef"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }
        
}
