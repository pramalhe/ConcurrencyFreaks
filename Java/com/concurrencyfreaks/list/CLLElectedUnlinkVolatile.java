package com.concurrencyfreaks.list;

import java.lang.reflect.Field;

/**
 * ****************************************************************************
 * Copyright (c) 2013-2014, Pedro Ramalhete and Andreia Correia All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation 
 * and/or other materials provided with the distribution. 
 * Neither the name of the author nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *****************************************************************************
 */


/**
 * <h1>Concurrent Linked List with Elected Unlink</h1>
 * This is exactly the same as CLLElectedUnlink but the "next" of the node is volatile instead of relaxed.
 * the goal of this data structure is to compare the two methods on a benchmark.
 * 
 * A Linked List where add() are always done at the end of the list
 * and the remove() can be done anywhere but the unlinking operation
 * is done with an Elected pattern.
 * <p>
 * This implementation supports three major operations, whose Progress 
 * Conditions are:
 * <ul>
 * <li>{@code add()} - Lock-Free
 * <li>{@code remove()} - Lock-Free
 * <li>{@code contains()} - Lock-Free
 * </ul>
 * <p>
 * <ul>
 * There are at least two non-trivial details in this data structure:
 * <li> <strong>Election Mechanism</strong> -
 *      This is a "Special One Pattern", also called "Election Pattern", and
 *      it consists of using an atomic variable, {@code unlinkGuard}, to 
 *      <i>elect</i> a thread that will be given a <i>special</i> task. 
 *      In this case, the special task is to unlink nodes whose {@code state}
 *      is {@code REMOVED}.
 * 
 * <li> <strong>List Traversal with Relaxed Atomics</strong> -
 *      The member Node.next is not qualified as a {@code volatile}, instead, 
 *      this is a <b>relaxed atomic</b>. We have atomicity guarantees from 
 *      the JVM for Node.next, and we only need to do an acquire-barrier if 
 *      Node.next is null, because Node.next can only take two values: null 
 *      (initial value), or a reference to the next Node instance in the list. 
 *      Once it has been assigned a non-null reference, the Node.next can only 
 *      be assigned another non-null reference.            
 * </ul>
 *
 * Compared to java.util.concurrent.ConcurrentLinkedQueue, this data structure 
 * is not as "GC friendly" because each node that is unlinked, has a 
 * {@code next} that references another node that is possibly still in the 
 * linked list.
 * One way to address this is to use the same trick that the CLQ uses, and set
 * the {@code next} to itself after a node is unlinked. This would force 
 * traversal operations to restart from the head if they find a {@code next} 
 * that points to the same node. See CLQ.contains() and CLQ.succ() for an 
 * example of how this works.
 * <p>
 * The main advantage of this algorithm when compared with previously known
 * lock-free lists, is that the traversal of the list can be done without 
 * any barriers, at least until a {@code next} with null or a matching 
 * {@code item} are found.
 * This means that for algorithms like the one on the CLQ, the traversal of
 * the list implies O(n) volatile loads, while for the CLLElectedUnlink 
 * algorithm it is O(1) volatile loads.
 * On architectures like x86, where a volatile load (an acquire barrier in 
 * C++1x terms) comes for free, this won't make any difference in terms of
 * performance, but in architectures with a more relaxed model where the 
 * acquires have a price (like ARM), this may be able to provide some gains.
 * How big are those gains depends a lot on the difference in <i>cost</i>
 * between using a volatile load (load with acquire barrier) or a regular 
 * load (relaxed atomic load) on that particular architecture. 
 * <p>
 * More info at:
 * http://concurrencyfreaks.com/2014/06/cllelectedunlink-lock-free-list-with.html
 * <p>
 * @author Andreia Correia
 * @author Pedro Ramalhete
 */
public class CLLElectedUnlinkVolatile<E> implements java.io.Serializable {

    private static final long serialVersionUID = -7469378984991097282L;
    
    // Possible states of Node.state
    private static final int INUSE   = 0;
    private static final int REMOVED = 1;
    
    // Possible states of unlinkGuard
    private static final long NO_GUARD = 0;
    private static final long GUARDED  = 1;
    
    // Possible states of unlinkNeeded
    private static final long NO_NEED     = 0;
    private static final long NEED_UNLINK = 1;
    
    
    /* Instance variables */
    
    /**
     * This is the "lock" of the Elected Pattern that protects the unlinking
     * operation. Its states can be NO_GUARD or GUARDED.
     * For more details, see this this post on the Elected Pattern:
     * http://concurrencyfreaks.com/2014/06/elected-pattern.html
     */
    private volatile long unlinkGuard;
    
    /**
     * This is a flag to tell the next remove() if an unlink is needed or not.
     * States can be NO_NEED or NEED_UNLINK.
     */
    private volatile long unlinkNeeded;
    
    /**
     * Reference to the Node at the start of the list. Traversals of the linked 
     * list start from this node. 
     */
    private volatile Node<E> head;
    /**
     * Reference to the Node at the end of the list. Used by add() to insert new
     * Nodes into the linked list.
     */
    private volatile Node<E> tail;
    
    /**
     * Inner class for Nodes
     * <p>
     * Some notes on the member variables:
     * <ul> 
     * <li> {@code item} is final because once assigned, we don't want to 
     * change it. This will save us some acquire barriers.
     * <li> {@code next} is <b>not</b> volatile because we only want to do the 
     * acquire and release barriers in certain places. We don't want to force 
     * an acquire barrier when traversing the list, until we reach the 
     * original tail.
     * <li> {@code state} is volatile and will be modified only with a CAS, so 
     * that only one thread can set it to {@code REMOVED}. Possible states of 
     * this variable are {@code INUSE} or {@code REMOVED}.
     * </ul>
     */
    static class Node<E> {
        final E item;
        volatile Node<E> next;
        volatile int state;
        
        Node(E item) {
            this.item = item;
            this.next = null;
            this.state = INUSE;
        }
    
        /**
         * Set Node's state to {@code REMOVED}
         * 
         * @return true if CAS was successful and this thread was the 
         * responsible for doing the (logical) removal of the element from
         * the linked list.
         */
        boolean markRemoved() {
            return UNSAFE.compareAndSwapInt(this, stateOffset, INUSE, REMOVED);
        }
        
        /**
         * Checks if the Node has been marked as REMOVED already.
         * @return true if the Node is already marked as REMOVED
         */
        boolean isRemoved() {
            return (state == REMOVED);
        }
        
        /**
         * This will modify Node.next, which is not volatile, but 
         * compareAndSwapObject() will take care of doing the release-barrier.
         * 
         * @param cmp Previous next
         * @param val New next
         * @return {@code true} if CAS was successful
         */
        boolean casNext(Node<E> cmp, Node<E> val) {
            return UNSAFE.compareAndSwapObject(this, nextOffset, cmp, val);
        }
        
        // Unsafe mechanics
        private static final sun.misc.Unsafe UNSAFE;
        private static final long stateOffset;
        private static final long nextOffset;

        static {
            try {
                Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
                f.setAccessible(true);
                UNSAFE = (sun.misc.Unsafe) f.get(null);
                //UNSAFE = sun.misc.Unsafe.getUnsafe();
                Class<?> k = Node.class;
                stateOffset = UNSAFE.objectFieldOffset
                        (k.getDeclaredField("state"));
                nextOffset = UNSAFE.objectFieldOffset
                        (k.getDeclaredField("next"));
            } catch (Exception e) {
                System.out.println("In Eclipse, add -Xbootclasspath/a:./bin/ to fix this exception\n");                
                throw new Error(e);
            }
        }
    }

    
    
    /**
     * Default constructor.
     */
    public CLLElectedUnlinkVolatile() {
        unlinkGuard  = NO_GUARD;
        unlinkNeeded = NO_NEED;
        // Start with the head and tail having a sentinel node
        head = tail = new Node<E>(null);
    }

    
    /**
     * Adds {@code item} to the end of the list. 
     * 
     * Progress Condition: Lock-Free - Expected <i>O(1)</i>
     * 
     * @param item
     */
    public boolean add(final E item) {
        checkNotNull(item);
        final Node<E> newNode = new Node<E>(item);
        while (true) {
            final Node<E> localTail = tail;
            final Node<E> node = localTail.next;
            if (localTail == tail) {
                if (node == null) {
                    // It seems this is the last node, so add the newNode here 
                    // and try to move the tail to the newNode
                    if (localTail.casNext(null, newNode)) {
                        casTail(localTail, newNode); // Failure is OK.
                        return true;
                    }
                } else {
                    casTail(localTail, node);
                }
            }
        }
    }
    
    
    /**
     * Search for {@code item} in the list.
     * <p>
     * Progress Condition: Lock-Free - <i>Expected O(N<sub>Nodes</sub>)</i>
     * 
     * @param item
     * @return {@code true} if {@code item} is in the list, and {@code false} 
     * if it is not there, or is there but marked as {@code REMOVED}
     */
    public boolean contains(final E item) {
        if (item == null) return false;
        Node<E> node = head;
        while (node != null) {
            if (item.equals(node.item) && node.state == INUSE) {
                return true;
            }
            node = node.next;
        }
        return false;
    }
    
    
    /**
     * Logically removes elements from the list.
     * <p>
     * When searching the list, uses the same optimization of relaxed atomics 
     * as contains().
     * <p>
     * Progress Condition: Lock-Free - Expected <i>O(N<sub>Nodes</sub>)</i>
     * 
     * @param item
     * @return {@code true} if {@code item} was found and was not already in 
     * {@code REMOVED} state and was successfully set to {@code REMOVED}
     */
    public boolean remove(final E item) {
        checkNotNull(item);
        if (unlinkGuard == NO_GUARD && casUnlinkGuard(NO_GUARD, GUARDED)) {
            try {
                // We got the hold on the guard, now figure out if unlinking is
                // needed for other nodes or just this one.
                if (unlinkNeeded == NEED_UNLINK) {
                    return markOneAndUnlinkAll(item);               
                } else {
                    return markAndUnlinkOne(item);
                }
            } finally {
                unlinkGuard = NO_GUARD;
            }
        } else {
            // Didn't get the hold of the guard, so mark the node and don't
            // do any unlinking.
            return markNode(item);
        }
    }
    
    
    /**
     * Check each node on the list and unlink all whose's state is {@code REMOVED}.
     * <p>
     * This should be called only from {@code remove()} and under the 
     * protection of {@code unlinkGuard}.
     * <p>
     * Progress Condition: Lock-Free 
     * 
     * @param item
     * @return
     */
    private boolean markOneAndUnlinkAll(final E item) {
        casUnlinkNeeded(NEED_UNLINK, NO_GUARD); 
        Node<E> node = head;
        Node<E> lastInUse = head;
        final Node<E> localTail = tail;
        boolean doUnlinking = true; // Turns to false once the localTail has been reached
        boolean nodeWasMarked = false;
        
        // Lets loop first to advance the head if needed
        while (head.state == REMOVED) {
            Node<E> headNext = head.next; 
            if (headNext == null) return false;
            head = headNext;
        }
        node = head;
        
        while (node != null) {
            // We do the order here differently and read the value of "state"
            // before looking at the key.
            if (!nodeWasMarked && node.state == INUSE && item.equals(node.item) && node.markRemoved()) {
                // We've marked the node, but instead of returning true 
                // immediately, we continue to search the linked list for 
                // nodes that need to be unlinked.
                nodeWasMarked = true;
                // Unlink only if it is not the last node nor the first
                if (lastInUse != head && node.next != null) {
                    lastInUse.next = node.next;
                }
            }            
            // Once the tail has been found, don't do any more unlinking 
            if (node == localTail) doUnlinking = false;
            // Optimization: If the tail has been reached and the item has been 
            // found, then we can return true immediately.
            if (!doUnlinking && nodeWasMarked) return true;
            if (doUnlinking) {
                if (node.state == INUSE) {
                    lastInUse = node;
                } else {
                    lastInUse.next = node.next;
                }
            }
            node = node.next;            
        }            
        return nodeWasMarked;
    }

    
    /**
     * Find the node with "item", mark it and unlink it.
     * <p>
     * When there is no other unlink to do, then go over the linked list 
     * searching for the node with "item", mark that node and unlink it.
     * Don't even try to look for unlinked nodes because that will make
     * the operation slower.
     * <p>
     * This should be called only from {@code remove()} and under the 
     * protection of {@code unlinkGuard}.
     * <p>
     * Progress Condition: Lock-Free 
     * 
     * @param item
     * @return
     */
    private boolean markAndUnlinkOne(final E item) {
        Node<E> node = head;
        Node<E> prev = head;
        while (node != null) {
            if (item.equals(node.item) && node.state == INUSE) {
                if (node.markRemoved()) {
                    if (node == head && node.next != null) {
                        // Removed node was "head" so try to advance it
                        casHead(node, node.next);
                    } else if (node.next != null) {
                        // Unlink only if it is not the last node
                        prev.next = node.next;
                    } 
                    return true;
                }
            }
            prev = node;
            node = node.next;            
        }            
        return false;
    }
            
    
    /**
     * Find only the node with "item" and mark it
     * <p>
     * Progress Condition: Lock-Free 
     *         
     * @param item
     * @return
     */
    private boolean markNode(final E item) {
        Node<E> node = head;
        while (node != null) {
            if (item.equals(node.item) && node.state == INUSE) {
                if (node.markRemoved()) {
                    unlinkNeeded = NEED_UNLINK;
                    return true;
                }
            }
            node = node.next;            
        }
        return false;
    }
    
        
    /**
     * Throws NullPointerException if argument is null.
     *
     * @param v the element
     */
    static void checkNotNull(Object v) {
        if (v == null)
            throw new NullPointerException();
    }

    boolean casUnlinkGuard(long cmp, long val) {
        return UNSAFE.compareAndSwapLong(this, guardOffset, cmp, val);
    }

    boolean casUnlinkNeeded(long cmp, long val) {
        return UNSAFE.compareAndSwapLong(this, unlinkNeededOffset, cmp, val);
    }
    
    boolean casTail(Node<E> cmp, Node<E> val) {
        return UNSAFE.compareAndSwapObject(this, tailOffset, cmp, val);
    }

    boolean casHead(Node<E> cmp, Node<E> val) {
        return UNSAFE.compareAndSwapObject(this, headOffset, cmp, val);
    }    
    
    // Unsafe mechanics
    private static final sun.misc.Unsafe UNSAFE;
    private static final long guardOffset;
    private static final long unlinkNeededOffset;
    private static final long tailOffset;
    private static final long headOffset;

    static {
        try {
            Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            UNSAFE = (sun.misc.Unsafe) f.get(null);
            //UNSAFE = sun.misc.Unsafe.getUnsafe();
            Class<?> k = CLLElectedUnlinkVolatile.class;
            guardOffset = UNSAFE.objectFieldOffset
                    (k.getDeclaredField("unlinkGuard"));
            unlinkNeededOffset = UNSAFE.objectFieldOffset
                    (k.getDeclaredField("unlinkNeeded"));
            tailOffset = UNSAFE.objectFieldOffset
                    (k.getDeclaredField("tail"));
            headOffset = UNSAFE.objectFieldOffset
                    (k.getDeclaredField("head"));
        } catch (Exception e) {
            System.out.println("In Eclipse, add -Xbootclasspath/a:./bin/ to fix this exception\n");
            throw new Error(e);
        }
    }
    
}
