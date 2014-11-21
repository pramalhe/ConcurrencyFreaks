/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
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
import java.util.concurrent.atomic.AtomicReference;
import sun.misc.Contended;


/** <h1>Left-Right pattern TreeSet with a per-Reader-Version</h1>
 * A Thread-safe TreeSet data-structure that has 
 * Wait-Free-Population-Oblivious properties for contains().
 * Follows the algorithm described in the paper "Left-Right: A Concurrency 
 * Control Technique with Wait-Free Population Oblivious Reads".
 * <p>
 * Uses the "Reader's Version" variant described in section 3.3 of the 
 * Left-Right paper.
 * <p> 
 * One (theoretical) problem that should eventually be solved is that the version/state 
 * of each Reader may be incremented indefinitely and eventually overflow.
 * In practice, it would take 50000 years for that to happen, so it's ok. 
 * 
 * @author Andreia Correia
 * @author Pedro Ramalhete
 * @see java.util.TreeSet
 */
public class LRScalableTreeSetRV<E> implements java.io.Serializable {
	
    private static final long serialVersionUID = -8299426488202724443L;

    // Initial Readers state
    private final static int STATE_NOT_READING  = -1;
    // States of leftRight
    private final static int READS_ON_LEFT      = -1;
    private final static int READS_ON_RIGHT     = 1;

    private final static State[] dummyArray = new State[0];    
    
    private final TreeSet<E> leftTree;
    private final TreeSet<E> rightTree;

    @Contended
    private transient volatile int leftRight;
    private transient int writerLeftRight;    

    private transient final ConcurrentLinkedQueue<State> readersStateQ;
    
    private transient final AtomicReference<State[]> readersStateArray;

    private transient final ThreadLocal<ReadersEntry> entry;

    private transient final State[] writeState;
    
    private transient final long[] writeStateInitial = new long[128];
   
        
    /**
     * We want each instance of State to be @Contended so that they
     * don't share cache-lines among themselves, which could cause
     * false-sharing of State.readerState among the different instances.
     */ 
    @Contended
    public static class State {
    	// Possible states for this variable are:
    	// -1:                Not reading, initial state
    	// Negative number x: Not reading, last version was x
    	// Positive number x: Currently reading with version x
        private volatile long readerState;
        private long localreaderState;
        
        public State() {
            this.readerState = STATE_NOT_READING;
            this.localreaderState = STATE_NOT_READING;
        }
        
        public State(long readerState) {
            this.readerState = readerState;
            this.localreaderState = readerState;
        }
    }
    
    /**
     * Inner class that makes use of finalize() to remove the Reader's state 
     * from the ConcurrentLinkedQueue {@code readersStateQ}
     */
    private class ReadersEntry {
        public final State st;
        
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
    public LRScalableTreeSetRV() {
        
        leftTree  = new TreeSet<E>();
        rightTree = new TreeSet<E>();
        
        // Only "Write" operations can modify leftRight
        leftRight = READS_ON_LEFT;
        writerLeftRight = READS_ON_LEFT;

        // Stores the Reader's state for each thread
        readersStateQ = new ConcurrentLinkedQueue<State>();

        readersStateArray = new AtomicReference(null);
        
        entry = new ThreadLocal<ReadersEntry>();

        writeState = new State[128];
    }

    /** 
     * This function should be called only from ReadersEntry.finalize()
     * 
     * @param removeTicket The reader's states that we wish to remove from the ConcurrentLinkedQueues
     */
    protected void removeState(State st) {        
        // We don't need to lock in exclusive mode thanks to the GC
        st.readerState = STATE_NOT_READING;
        readersStateQ.remove(st);
        readersStateArray.set(null);
    }
    
    
    /**
     * Creates a new ReadersEntry instance for the current thread and
     * stores the associated State in the queue of states.
     */
    private ReadersEntry addState() {
        final ReadersEntry newEntry = new ReadersEntry(STATE_NOT_READING);        
        entry.set(newEntry);        
        readersStateQ.add(newEntry.st);
        readersStateArray.set(null);
        return newEntry;
    }
    
    /**
     * The arrays readersVersion0ArrayRef and readersVersion1ArrayRef
     * will only be rebuild if new thread is added or removed
     */
    private State[] rebuildArraysIfNeeded() {
        State[] localReadersStateArray = readersStateArray.get();
        if (localReadersStateArray == null) {
            // Set to dummyArray before copying readersStateQ
            readersStateArray.set(dummyArray);
            // Copy readersStateList to an array
            localReadersStateArray = readersStateQ.toArray(new State[readersStateQ.size()]);
            readersStateArray.compareAndSet(dummyArray, localReadersStateArray);
        }
        
        return localReadersStateArray;
    }
    
    private int scanInitialReaders() {
        final State[] localReadersVersionArray = rebuildArraysIfNeeded();
        
        int i=0;
        // Wait for Readers from next version
        for (State readerState : localReadersVersionArray) {
            
            if (readerState!= null){
                /*
                * gets the initial version of the Reader
                */
                long initialstate = readerState.readerState;
                if(initialstate > STATE_NOT_READING){
                    writeState[i] = readerState;
                    writeStateInitial[i] = initialstate;
                    i++;
                }
            }
        }
        return i;
    }    
    
    
    /**
     * Waits for all the threads doing a "Read" to finish their tasks on the 
     * TreeMap that the "Write" wants to modify.  
     * Must be called only by "Write" operations, and it {@code writeLock} must 
     * be locked when this function is called.
     */
    private void scanReaders() {
        
        final int length = scanInitialReaders();
        
        for(int j=0;j<length;j++){
            
            /*
             * If the state is positive or the state didn't change since the 
             * arrival of the Writer then it means that the Reader is performing
             * a read in one of the trees and it could be the tree were the Writer
             * wants to enter. Once the version of the Reader changes it means that
             * the read has to be performing on the tree opposite to where the Writer
             * wants to enter. If the state is negative it means that there is
             * no read associated with that thread at this moment.
             */
            State wstate = writeState[j];
            long initialstate = writeStateInitial[j];
            long state = wstate.readerState;
            while (state > STATE_NOT_READING && state == initialstate){
                Thread.yield();
                state = wstate.readerState;
            }
        }
    }   


    /**
     * 
     * Performance: 1 ThreadLocal.get() + 1 Atomic.get() + 2 Atomic.set()
     * 
     * @param elem
     * @return
     */
    public boolean contains(E elem) {
        ReadersEntry localReadersEntry = entry.get();
        // Initialize a new Reader-state for this thread if needed         
        if (localReadersEntry == null) {
            localReadersEntry = addState();
        }
        
        final long localReaderState = localReadersEntry.st.localreaderState;    
        // "Publish" a positive readerState to signal to Writer that this 
        // Reader is in-progress
        localReadersEntry.st.readerState = -localReaderState+1;
        // Read the up-to-date value of leftRight.
        final boolean retValue;
        if (leftRight == READS_ON_LEFT) {
            retValue = leftTree.contains(elem);
        } else {
            retValue = rightTree.contains(elem);
        }
        localReadersEntry.st.readerState = localReaderState-1;
        localReadersEntry.st.localreaderState = localReaderState-1;
        return retValue;
    }


    /**
     * This is a "Write" operation that inserts an element into the Tree(s). 
     * Notice this operation is synchronized
     *
     * @return Returns {@code true} if {@code elem} was found and removed, and 
     * {@code false} otherwise.
     */
    public synchronized boolean add(E elem) {
        final boolean retValue;
        // Do the put() in the first Tree, opposite to the one currently 
        // being used by the Read operations. No need to wait.
        if (writerLeftRight == READS_ON_LEFT) {
            retValue = rightTree.add(elem);
        } else {
            retValue = leftTree.add(elem);
        }
        // Optimization that only works for Sets
        if (!retValue) return false;

        writerLeftRight = -writerLeftRight;
        leftRight = writerLeftRight;
        scanReaders();

        if (writerLeftRight == READS_ON_LEFT) {
            rightTree.add(elem);
        } else {
            leftTree.add(elem);
        }
        return retValue;
    }


    /**
     * Removes the element from this set if present
     *  
     * This is a "Write" operation that removes an element from the Tree(s).
     * 
     * @return Returns {@code true} if {@code elem} was found and removed, and 
     * {@code false} otherwise.
     */
    public synchronized boolean remove(E elem) {	
        final boolean retValue;
        // Do the remove() in the first Tree, opposite to the one currently 
        // being used by the Read operations. No need to wait.
        if (writerLeftRight == READS_ON_LEFT) {
            retValue = rightTree.remove(elem);
        } else {
            retValue = leftTree.remove(elem);
        }
        // Optimization that only works for Sets
        if (!retValue) return false;
      
        writerLeftRight = -writerLeftRight;
        // Does an atomic set()
        leftRight = writerLeftRight;
        scanReaders();
		
        if (writerLeftRight == READS_ON_LEFT) {
        	rightTree.remove(elem);
        } else {
        	leftTree.remove(elem);
        }		
        return retValue;
    }
        
}

