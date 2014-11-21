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
import java.util.concurrent.atomic.AtomicLongArray;

/**
 * <h1>Left-Right pattern TreeSet using the Scalable GT variant</h1> 
 * A Thread-safe TreeSet data-structure that has 
 * Wait-Free-Population-Oblivious properties for contains(). 
 * <p> Uses a modified (scalable) variant of the ReadIndicator used by Gil Tene in WriterReaderPhaser:
 * http://stuff-gil-says.blogspot.fr/2014/11/writerreaderphaser-story-about-new.html
 * <p>
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 * @see TreeSet
 */
public class LRTreeSetScalableGT<E> implements java.io.Serializable {

    private static final long serialVersionUID = -2024253784199310652L;

    // States of the leftRight variable
    private final static int READS_ON_LEFT = -1;
    private final static int READS_ON_RIGHT = 1;

    private final TreeSet<E> leftTree;
    private final TreeSet<E> rightTree;
    private transient final AtomicLong leftRight;

    // ReadIndicator + versionIndex:
    private transient final InnerDCLC startEpoch = new InnerDCLC(0);
    private transient final InnerDCLC posEndEpoch = new InnerDCLC(0);
    private transient final InnerDCLC negEndEpoch = new InnerDCLC(Long.MIN_VALUE);


    /**
     * This is a specialization of the Distributed Cache Line Counter class
     *
     */
    static class InnerDCLC {
        // Size of the counters[] array (TODO: explain the magical number <<2)
        private final static int kNumCounters = Integer.highestOneBit(Runtime.getRuntime().availableProcessors())<<2;
        
        // Size of a cache line in ints
        private final static int COUNTER_CACHE_LINE = 64/8;    
        
        // Stores the number of readers holding the read-lock 
        private final AtomicLongArray counters = new AtomicLongArray(kNumCounters*COUNTER_CACHE_LINE);

        public InnerDCLC(long value) {
            for (int idx = 0; idx < kNumCounters*COUNTER_CACHE_LINE; idx += COUNTER_CACHE_LINE) {
                counters.set(idx, value);
            }
        }
        
        private int tid2hash() {
            long x = Thread.currentThread().getId();
            x ^= (x << 21);
            x ^= (x >>> 35);
            x ^= (x << 4);
            final int idx = (int)(((kNumCounters-1) & x)*COUNTER_CACHE_LINE);
            return idx;
        }
        
        /**
         * Notice that we only care if the return value is positive or negative, so as
         * to determine the versionIndex
         */
        public long getAndIncrement() {
            return counters.getAndIncrement(tid2hash());
        }
        
        public long sum() {
            long sum = 0;
            // Let's start by checking if the versionIndex is positive or negative
            if (counters.get(0) < 0) {
                for (int idx = 0; idx < kNumCounters*COUNTER_CACHE_LINE; idx += COUNTER_CACHE_LINE) {
                    sum += (counters.get(idx) - Long.MIN_VALUE);
                }
                return Long.MIN_VALUE + sum;
            } else {
                for (int idx = 0; idx < kNumCounters*COUNTER_CACHE_LINE; idx += COUNTER_CACHE_LINE) {
                    sum += counters.get(idx);
                }
                return sum;            
            }
        }
        
        public void set(long value) {
            for (int idx = 0; idx < kNumCounters*COUNTER_CACHE_LINE; idx += COUNTER_CACHE_LINE) {
                counters.set(idx, value); 
            }
        }    
        
        /**
         * Warning: This is not an atomic getAndSet(). It sets every counter in the array
         * to 'value' and returns the accumulated delta.
         */
        public long getAndSet(long value) {
            long sum = 0;
            // Let's start by checking if the versionIndex is positive or negative
            if (counters.get(0) < 0) {
                for (int idx = 0; idx < kNumCounters*COUNTER_CACHE_LINE; idx += COUNTER_CACHE_LINE) {
                    sum += (counters.getAndSet(idx, value) - Long.MIN_VALUE);
                }
                return Long.MIN_VALUE + sum;
            } else {
                for (int idx = 0; idx < kNumCounters*COUNTER_CACHE_LINE; idx += COUNTER_CACHE_LINE) {
                    sum += counters.getAndSet(idx, value);
                }
                return sum;            
            }
        }
    }
    
    
    
    /**
     * Default constructor.
     */
    public LRTreeSetScalableGT() {
        leftTree = new TreeSet<E>();
        rightTree = new TreeSet<E>();
        leftRight = new AtomicLong(READS_ON_LEFT);
    }


    /**
     * Called by the Reader before entering the critical section
     */
    private long arrive() {
        return startEpoch.getAndIncrement();
    }
    
    
    /**
     * Called by the Reader after leaving the critical section
     * @param localVI versionIndex seen by arrive()
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
     */
    private void toggleVersionAndScan() {
        final long localVI = startEpoch.sum();
        if (localVI < 0) {
            // This version is negative, so next versionIndex is positive. 
            // Reset counter
            posEndEpoch.set(0);
            // Toggle versionIndex and count the number of arrives()
            final long localStartValue = startEpoch.getAndSet(0);
            // Wait for the readers that did arrive() with a negative versionIndex to depart()
            while (localStartValue != negEndEpoch.sum()) Thread.yield();
        } else {
            // This version is positive, so next versionIndex is negative. 
            // Reset counter
            negEndEpoch.set(Long.MIN_VALUE);
            // Toggle versionIndex and count the number of arrives()
            final long localStartValue = startEpoch.getAndSet(Long.MIN_VALUE);
            // Wait for the readers that did arrive() with a positive versionIndex to depart()
            while (localStartValue != posEndEpoch.sum()) Thread.yield();
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
