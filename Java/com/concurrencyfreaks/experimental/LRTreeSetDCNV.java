package com.concurrencyfreaks.experimental;

/**
 * ****************************************************************************
 * Copyright (c) 2011-2014, Pedro Ramalhete and Andreia Correia All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. * Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials provided
 * with the distribution. * Neither the name of the author nor the names of its
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
import java.util.TreeSet;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.LongAdder;

/**
 * <h1>Left-Right pattern TreeSet using the Distributed Counter No Version variant</h1> 
 * A Thread-safe TreeSet data-structure that has 
 * Wait-Free-Population-Oblivious properties for contains(). 
 * <p> 
 * This variant is somewhat similar to the GT variant because it uses three 
 * counters but it's the leftRight that's combined with the ingress and not
 * the versionIndex, which is a good improvement from an algorithmic point 
 * of view.
 * <p>
 * For more explanations see this post:
 * http://concurrencyfreaks.com/2014/12/left-right-distributed-counters-no.html
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 * @see TreeSet
 */
public class LRTreeSetDCNV<E> implements java.io.Serializable {

    private static final long serialVersionUID = -91670989910100205L;

    // States of the leftRight variable
    private final static int READS_ON_LEFT = 0;   // positive
    private final static int READS_ON_RIGHT = -1; // negative

    // Notice that these are not volatile
    private final TreeSet<E> leftTree;
    private final TreeSet<E> rightTree;

    // leftRight + ingress counter:
    private transient final AtomicLong leftRightIngress = new AtomicLong(0);
    // Egress counters
    private transient final LongAdder leftEgress = new LongAdder();
    private transient final LongAdder rightEgress = new LongAdder();


    /**
     * Default constructor.
     */
    public LRTreeSetDCNV() {
        leftTree = new TreeSet<E>();
        rightTree = new TreeSet<E>();
    }

    private int getLeftRight(long localLeftRightIngress) {
        return (localLeftRightIngress >= 0) ? READS_ON_LEFT : READS_ON_RIGHT;
    }

    /**
     * Called by the Reader before entering the critical section
     * <p>
     * Progress Condition: Wait-Free Population Oblivious on x86, Lock-Free for other CPUs
     * <p>
     * @return Returns the current leftRight
     */
    private int arrive() {
        return getLeftRight(leftRightIngress.getAndIncrement());
    }
    
    
    /**
     * Called by the Reader after leaving the critical section
     * @param localLeftRight the leftRight variable seen by arrive()
     * <p>
     * Progress Condition: Wait-Free Population Oblivious on x86, Lock-Free for other CPUs
     */
    private void depart(int localLeftRight) {
        if (localLeftRight == READS_ON_LEFT) {
            leftEgress.increment();
        } else {
            rightEgress.increment();
        }
    }
    
    
    /**
     * Although the method is called "toggleVersionAndWait()", it is actually 
     * toggling the {@code leftRight} and not the versionIndex, but we keep the
     * name for consistency with the other Left-Right variants.
     * 
     * This method must be called within the protection of a mutual exclusion 
     * lock {@code writersMutex}. 
     */
    private void toggleVersionAndWait() {
        final int localLeftRight = getLeftRight(leftRightIngress.get());
        if (localLeftRight == READS_ON_LEFT) {
            rightEgress.reset();
            // Toggle leftRight and count the number of arrives()
            final long lri = leftRightIngress.getAndSet(Long.MIN_VALUE);
            // Wait for the corresponding readers that did arrive()
            while (lri != leftEgress.sum()) Thread.yield();
        } else {
            leftEgress.reset();
            final long lri = leftRightIngress.getAndSet(0);
            while (lri+Long.MIN_VALUE != rightEgress.sum()) Thread.yield();
        }
    }


    /**
     * A "Read" operation that returns an element.
     *
     * @return Returns the value to which the specified key is mapped, or
     * {@code null} if this map contains no mapping for the key.
     */
    public boolean contains(E elem) {
        final int localLeftRight = arrive();
        try {
            if (localLeftRight == READS_ON_LEFT) {
                return leftTree.contains(elem);
            } else {
                return rightTree.contains(elem);
            }
        } finally {
            // In the extreme event that TreeSet.contains() throws an exception, 
            // we want to make sure that no Writer is left hanging.
            depart(localLeftRight);
        }
    }

    
    /**
     * This is a "Write" operation that inserts an element into the Tree(s). The
     * {@code writeLock} will be locked throughout the entire operation. <p>
     *
     * @param elem element to be added to this set
     * @return {@code true} if this set did not already contain the specified
     * element
     */
    public synchronized boolean add(E elem) {
        final boolean retValue;
        final int localLeftRight = getLeftRight(leftRightIngress.get());
        // Do the add() in the first Tree, opposite to the one currently 
        // being used by the Read operations. No need to wait.                
        if (localLeftRight == READS_ON_LEFT) {
            retValue = rightTree.add(elem);
        } else {
            retValue = leftTree.add(elem);
        }
        // Optimization that only works for Sets
        if (!retValue) return false;

        // Toggle leftRight and wait for currently running Readers
        toggleVersionAndWait();

        if (localLeftRight == READS_ON_LEFT) {
            leftTree.add(elem);
        } else {
            rightTree.add(elem);
        }

        return retValue;
    }

    
    /**
     * Removes the specified element from this set if it is present. More
     * formally, removes an element {@code e} such that
     * <tt>(o==null&nbsp;?&nbsp;e==null&nbsp;:&nbsp;o.equals(e))</tt>, if this
     * set contains such an element. Returns {@code true} if this set contained
     * the element (or equivalently, if this set changed as a result of the
     * call). (This set will not contain the element once the call returns.) <p>
     * The {@code writeLock} will be locked throughout the entire operation.
     *
     * @param elem element to be removed from this set, if present
     * @return {@code true} if this set contained the specified element
     */
    public synchronized boolean remove(E elem) {
        final boolean retValue;
        final int localLeftRight = getLeftRight(leftRightIngress.get());
        // Do the add() in the first Tree, opposite to the one currently 
        // being used by the Read operations. No need to wait.                
        if (localLeftRight == READS_ON_LEFT) {
            retValue = rightTree.remove(elem);
        } else {
            retValue = leftTree.remove(elem);
        }
        // Optimization that only works for Sets
        if (!retValue) return false;

        // Toggle leftRight and wait for currently running Readers
        toggleVersionAndWait();

        if (localLeftRight == READS_ON_LEFT) {
            leftTree.remove(elem);
        } else {
            rightTree.remove(elem);
        }

        return retValue;
    }
}
