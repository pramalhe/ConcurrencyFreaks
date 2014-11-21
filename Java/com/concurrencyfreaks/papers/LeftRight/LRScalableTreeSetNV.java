package com.concurrencyfreaks.papers.LeftRight;
/******************************************************************************
 * Copyright (c) 2011-2014, Pedro Ramalhete and Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the
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

import java.util.TreeSet;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import sun.misc.Contended;


/** <h1>Wait-Free-Read Thread-Local-Optimized TreeMap  </h1>
 * A Thread-safe TreeSet-like data-structure 
 * that has Wait-Free-Population-Oblivious properties for contains()
 * Follows the algorithm described in the paper "Left-Right: A Concurrency 
 * Control Technique with Wait-Free Population Oblivious Reads".
 * <p>
 * Uses the "No Version" variant described in section 3.1 of the Left-Right
 * paper. 
 *
 * 
 * @author Andreia Correia
 * @author Pedro Ramalhete
 * @see java.util.TreeSet
 */
public class LRScalableTreeSetNV<E> implements java.io.Serializable {
	
    private static final long serialVersionUID = 3921715664815870671L;

    // Reader's state definitions
    private final static int STATE_NOT_READING   = 0;
    private final static int STATE_READING       = 2;
    
    private final static int READS_ON_LEFT   = -1;
    private final static int READS_ON_RIGHT  = 1;
    
    private final static State[] dummyArray = new State[0];
    
    private final TreeSet<E> leftTree;
    private final TreeSet<E> rightTree;
    
    // Possible states for this variable are: READS_ON_LEFT or READS_ON_RIGHT
    private transient final AtomicInteger leftRight;

    private transient final ConcurrentLinkedQueue<State> readersStatesQ;
    
    private transient final AtomicReference<State[]> readersStateArray;

    private transient final ThreadLocal<ReadersEntry> entry;

    // We want each instance of State to be @Contended so that they
    // don't share cache-lines among themselves, which could cause
    // false-sharing of State.readerState among the different instances
    @Contended
    private static class State {
        // Possible states for this variable are: STATE_NOT_READING, 
    	// STATE_READING_BOTH, STATE_READING_LEFT, STATE_READING_RIGHT
        private volatile long readerState;
        
        public State(long readerState) {
            this.readerState = readerState;
        }
    }
    
    /**
     * Inner class that makes use of finalize() to remove the Reader's state 
     * from the ConcurrentLinkedQueue {@code readersStatesQ}
     */
    private class ReadersEntry {
        private final State st;
        
        public ReadersEntry(long readerState) {
            st = new State(readerState);
        }
        protected void finalize() throws Throwable {
            removeState(this.st);
            super.finalize();
        }
    }     

    
    /**
     * Default constructor.
     */
    public LRScalableTreeSetNV() {
        
        leftTree  = new TreeSet<E>();
        rightTree = new TreeSet<E>();
        
        // Only "Write" operations can modify this
        leftRight    = new AtomicInteger(READS_ON_LEFT);

        // Stores the Reader's state for each thread
        readersStatesQ = new ConcurrentLinkedQueue<State>();

        readersStateArray = new AtomicReference(null);
        
        entry = new ThreadLocal<ReadersEntry>();
    }

    /** 
     * This function should be called only from ReadersEntry.finalize()
     * 
     * @param state The reader's states that we wish to remove from the ConcurrentLinkedQueues
     */
    protected void removeState(State st) {        
        // We don't need to lock in exclusive mode Java thanks to the GC
        st.readerState = STATE_NOT_READING;
        readersStatesQ.remove(st);
        readersStateArray.set(null);
    }
    
    
    /**
     * Creates a new ReadersEntry instance for the current thread and
     * its associated AtomicInteger to store the state of the Reader
     */
    private ReadersEntry addState() {
        final ReadersEntry newEntry = new ReadersEntry(STATE_NOT_READING);         
        entry.set(newEntry);        
        readersStatesQ.add(newEntry.st);
        readersStateArray.set(null);        
        return newEntry;
    }
    
    /*
     * The arrays readersVersion0ArrayRef and readersVersion1ArrayRef
     * will only be rebuild if new thread is added or removed
     */
    private State[] rebuildArraysIfNeeded() {
        State[] localReadersStateArray = readersStateArray.get();
        if (localReadersStateArray == null) {
            // Set to dummyArray before copying readersStateQ
            readersStateArray.set(dummyArray);
            // Copy readersStateList to an array
            localReadersStateArray = readersStatesQ.toArray(new State[readersStatesQ.size()]);
            readersStateArray.compareAndSet(dummyArray, localReadersStateArray);
        }
        
        return localReadersStateArray;
    }
    
	
    /**
     * Waits for all the threads doing a "Read" to finish their tasks on the 
     * TreeMap that the "Write" wants to modify.  
     * Must be called only by "Write" operations, and it {@code writeLock} must 
     * be locked when this function is called.
     */
    private void scanReaders(int localLeftRight) {
        
        final State[] localReadersVersionArray = rebuildArraysIfNeeded();
        
        // Wait for Readers from next version
        for (State readerState : localReadersVersionArray) {
            
            if (readerState!= null){
                while (readerState.readerState == STATE_READING || readerState.readerState == localLeftRight){
                    Thread.yield();
                }
            }
        }
    } 
    

    /**
     * A "Read" operation that checks whether or not an element is present in
     * the set.
     * <p>
     * With this technique the readerState can be at STATE_READING_BOTH indefinitely
     * (theoretically) which could block a waiting Writer even though the new Readers
     * will go into the new tree. Moreover, it has 3 release-barriers instead of
     * 2 release-barriers like the NV_Inc technique.
     * <p>
     * Performance: 1 ThreadLocal.get() + 1 Atomic.get() + 3 Atomic.set()
     * 
     * @return {@code true} if the element is in the set and {@code false} otherwise
     */
    public boolean contains(E elem) {
        ReadersEntry localReadersEntry = entry.get();
        // Initialize a new Reader-state for this thread if needed         
        if (localReadersEntry == null) {
            localReadersEntry = addState();
        }
        
        // Cache the pointers as optimization
        final State localState = localReadersEntry.st;      
        localState.readerState = STATE_READING;

        // Read the up-to-date value of leftRight.
        // Order is important: The {@code leftRight} value can only be 
        // read _after_ the counter has been incremented for the 
        // corresponding {@code versionIndex}.
        final int threadLocalLeftRight = leftRight.get();
        final boolean retValue;
        localState.readerState = threadLocalLeftRight;
        if (threadLocalLeftRight == READS_ON_LEFT) {
            retValue = leftTree.contains(elem);
        } else {
            retValue = rightTree.contains(elem);
        }
        localState.readerState = STATE_NOT_READING;      
        
        return retValue;
    }



    /**
     * This is a "Write" operation that inserts an element into the Tree(s). 
     * The {@code writeLock} will be locked throughout the entire operation.
     * 
     */
    public synchronized boolean add(E elem) {
        final boolean retValue;
        final int localLeftRight = leftRight.get();
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
        // Notice that when doing a "Read", we first read the 
        // {@code versionIndex} and then the {@code leftRight}, while 
        // doing the "Write" the order is reversed: first we toggle the
        // {@code leftRight} and then we toggle the {@code versionIndex}.		
        scanReaders(localLeftRight);

        if (-localLeftRight == READS_ON_LEFT) {
            rightTree.add(elem);
        } else {
            leftTree.add(elem);
        }
        return retValue;
    }


    /**
     * Removes the element from this tree if present.
     *  
     * This is a "Write" operation that removes an element from the Tree(s).
     * The {@code writeLock} will be locked throughout the entire operation.
     * 
     * @return ??? 
     */
    public synchronized boolean remove(E elem) {
        final boolean retValue;
        final int localLeftRight = leftRight.get();
        // Do the remove() in the first Tree, opposite to the one currently 
        // being used by the Read operations. No need to wait.
        if (leftRight.get() == READS_ON_LEFT) {
            retValue = rightTree.remove(elem);
        } else {
            retValue = leftTree.remove(elem);
        }
        // Optimization that only works for Sets
        if (!retValue) return false;

        // Toggle leftRight and wait for currently running Readers
        leftRight.set(-localLeftRight);
        // Notice that when doing a "Read", we first read the 
        // {@code versionIndex} and then the {@code leftRight}, while 
        // doing the "Write" the order is reversed: first we toggle 
        // the {@code leftRight} and then we increment {@code versionIndex}.		     
        scanReaders(localLeftRight);
		        
        if (-localLeftRight == READS_ON_LEFT) {
            rightTree.remove(elem);
        } else {
        	leftTree.remove(elem);
        }
        return retValue;
    }       
}
