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

/**
 * <h1>Left-Right pattern TreeSet using the GT variant</h1> 
 * A Thread-safe TreeSet data-structure that has 
 * Wait-Free-Population-Oblivious properties for contains(). 
 * <p> Uses the ReadIndicator discovered by Gil Tene in WriterReaderPhaser:
 * http://stuff-gil-says.blogspot.fr/2014/11/writerreaderphaser-story-about-new.html
 * <p>
 * For more explanations see this post:
 * http://concurrencyfreaks.com/2014/11/left-right-gt-variant.html
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 * @see TreeSet
 */
public class LRTreeSetGT<E> implements java.io.Serializable {

    private static final long serialVersionUID = -2024253784199310651L;

    // States of the leftRight variable
    private final static int READS_ON_LEFT = -1;
    private final static int READS_ON_RIGHT = 1;

    private final TreeSet<E> leftTree;
    private final TreeSet<E> rightTree;
    private transient final AtomicLong leftRight;

    // ReadIndicator + versionIndex:
    private transient final AtomicLong startEpoch = new AtomicLong(0);
    private transient final AtomicLong posEndEpoch = new AtomicLong(0);
    private transient final AtomicLong negEndEpoch = new AtomicLong(Long.MIN_VALUE);


    /**
     * Default constructor.
     */
    public LRTreeSetGT() {
        leftTree = new TreeSet<E>();
        rightTree = new TreeSet<E>();
        leftRight = new AtomicLong(READS_ON_LEFT);
    }


    /**
     * Called by the Reader before entering the critical section
     * <p>
     * Progress Condition: Wait-Free Population Oblivious on x86, Lock-Free for other CPUs
     */
    private long arrive() {
        return startEpoch.getAndIncrement();
    }
    
    
    /**
     * Called by the Reader after leaving the critical section
     * @param localVI versionIndex seen by arrive()
     * <p>
     * Progress Condition: Wait-Free Population Oblivious on x86, Lock-Free for other CPUs
     */
    private void depart(final long localVI) {
        if (localVI < 0) {
            negEndEpoch.getAndIncrement();
        } else {
            posEndEpoch.getAndIncrement();
        }
    }
    
    
    /**
     * Waits for all the threads doing a "Read" to finish their tasks on the
     * TreeMap that the "Write" wants to modify. 
     * Uses the Left-Right GT variant.
     * This method must be called within the protection of a mutual exclusion lock (or synchronized block). 
     */
    private void toggleVersionAndScan() {
        final long localVI = startEpoch.get();
        if (localVI < 0) {
            // This version is negative, so next versionIndex is positive. 
            // Reset counter
            posEndEpoch.set(0);
            // Toggle versionIndex and count the number of arrives()
            final long localStartValue = startEpoch.getAndSet(0);
            // Wait for the readers that did arrive() with a negative versionIndex to depart()
            while (localStartValue != negEndEpoch.get()) Thread.yield();
        } else {
            // This version is positive, so next versionIndex is negative. 
            // Reset counter
            negEndEpoch.set(Long.MIN_VALUE);
            // Toggle versionIndex and count the number of arrives()
            final long localStartValue = startEpoch.getAndSet(Long.MIN_VALUE);
            // Wait for the readers that did arrive() with a positive versionIndex to depart()
            while (localStartValue != posEndEpoch.get()) Thread.yield();
        }
    }


    /**
     * A "Read" operation that returns an element.
     *
     * @return Returns the value to which the specified key is mapped, or
     * {@code null} if this map contains no mapping for the key.
     */
    public boolean contains(E elem) {
        final long localVI = arrive();
        try {
            // Read the up-to-date value of leftRight.
            if (leftRight.get() == READS_ON_LEFT) {
                return leftTree.contains(elem);
            } else {
                return rightTree.contains(elem);
            }
        } finally {
            // In the extreme event that TreeSet.contains() throws an exception, 
            // we want to make sure that no Writer is left hanging.
            depart(localVI);
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
        final long localLeftRight = leftRight.get();
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
        leftRight.set(-localLeftRight);
        toggleVersionAndScan();

        if (-localLeftRight == READS_ON_LEFT) {
            rightTree.add(elem);
        } else {
            leftTree.add(elem);
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
        final long localLeftRight = leftRight.get();
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
        leftRight.set(-localLeftRight);
        toggleVersionAndScan();

        if (-localLeftRight == READS_ON_LEFT) {
            rightTree.remove(elem);
        } else {
            leftTree.remove(elem);
        }

        return retValue;
    }
}
