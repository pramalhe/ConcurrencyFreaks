package com.concurrencyfreaks.papers.LeftRight;

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
import java.lang.reflect.Field;
import java.util.TreeSet;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

import sun.misc.Contended;

/**
 * <h1>Left-Right pattern TreeSet with a CLQ</h1> 
 * A Thread-safe TreeSet data-structure that has 
 * Wait-Free-Population-Oblivious properties for contains(). 
 * <p> Follows the algorithm described in the paper of the Left-Right pattern, 
 * but uses a ConcurrentLinkedQueue to store the states of each Reader thread. 
 * The finalize() of the ReadersEntry class is responsible
 * to remove the corresponding entry in the CLQ whenever a thread terminates.
 * <p>
 *
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 * @see TreeSet
 */
public class LRScalableTreeSet<E> implements java.io.Serializable {

    private static final long serialVersionUID = -2024253784199310650L;

    // Reader's states
    private final static int STATE_NOT_READING = 0;
    private final static int STATE_READING     = 1;
    // States of the leftRight variable
    private final static int READS_ON_LEFT = -1;
    private final static int READS_ON_RIGHT = 1;
    // States of versionIndex
    private final static int VERSION0 = 0;
    private final static int VERSION1 = 1;
    private final State[] dummyArray = new State[0];
    private final TreeSet<E> leftTree;
    private final TreeSet<E> rightTree;
    private transient final AtomicInteger leftRight;
    private transient final AtomicLong versionIndex;
    private transient final ConcurrentLinkedQueue<State> readersStateQ;
    private transient final AtomicReference<State[]> readersStateArrayRef;
    private transient final ThreadLocal<ReadersEntry> entry;

    /**
     * Inner class for the state of the Reader
     */
    @Contended
    private static class State {
        // These variable can either be STATE_READING or STATE_NOT_READING
        @Contended
        volatile int v0State;
        @Contended
        volatile int v1State;

        State(int v0State, int v1State) {
            this.v0State = v0State;
            this.v1State = v1State;
        }
    }

    /**
     * Inner class that makes use of finalize() to remove the Reader's state
     * from the ConcurrentLinkedQueue {@code readersStateQ}
     */
    private class ReadersEntry {

        final State st;

        public ReadersEntry(State state) {
            this.st = state;
        }

        protected void finalize() throws Throwable {
            removeState(st);
            super.finalize();
        }
    }

    /**
     * Default constructor.
     */
    public LRScalableTreeSet() {
        leftTree = new TreeSet<E>();
        rightTree = new TreeSet<E>();

        // Only "Write" operations can modify these
        leftRight = new AtomicInteger(READS_ON_LEFT);
        versionIndex = new AtomicLong(VERSION0);

        // Stores the Reader's state for each thread
        readersStateQ = new ConcurrentLinkedQueue<State>();

        readersStateArrayRef = new AtomicReference<State[]>(null);

        entry = new ThreadLocal<ReadersEntry>();
    }

    /**
     * This function should be called only from ReadersEntry.finalize()
     *
     * @param state The reader's states that we wish to remove from the
     * ConcurrentLinkedQueues
     */
    protected void removeState(State state) {
        // We don't need to lock in exclusive mode thanks to the GC
        readersStateQ.remove(state);
        readersStateArrayRef.set(null);
    }

    /**
     * Creates a new ReadersEntry instance for the current thread and its
     * associated State to store the state of the Reader
     */
    private ReadersEntry addState() {
        final State state = new State(STATE_NOT_READING, STATE_NOT_READING);
        final ReadersEntry newEntry = new ReadersEntry(state);
        entry.set(newEntry);
        readersStateQ.add(state);
        readersStateArrayRef.set(null);
        return newEntry;
    }

    /**
     * The array readersStateArrayRef will only be rebuild if a new thread is
     * added or removed
     */
    private State[] rebuildArrayIfNeeded() {
        State[] localReadersStateArray = readersStateArrayRef.get();
        if (localReadersStateArray == null) {
            // Set to dummyArray before scanning the readersStateQ 
            readersStateArrayRef.set(dummyArray);
            // Copy the elements in readersStateQ to an array
            localReadersStateArray =
                    readersStateQ.toArray(new State[readersStateQ.size()]);
            readersStateArrayRef.compareAndSet(dummyArray, localReadersStateArray);
        }
        return localReadersStateArray;
    }

    /**
     * Waits for all the threads doing a "Read" to finish their tasks on the
     * TreeMap that the "Write" wants to modify. Must be called only by "Write"
     * operations, and it {@code mutex} must be locked when this function is
     * called.
     */
    private void toggleVersionAndScan() {
        State[] localReadersStateArray = rebuildArrayIfNeeded();
        final long localVersionIndex = versionIndex.get();
        final int prevVersionIndex = (int)(localVersionIndex % 2);
        final int nextVersionIndex = (int)((localVersionIndex+1) % 2);
        
        // Wait for Readers from next version
        for (State readerState : localReadersStateArray) {
            skipOrYield(readerState, nextVersionIndex);
        }
        
        // Toggle versionIndex
        versionIndex.set(localVersionIndex+1);

        // Wait for Readers from previous version
        for (State readerState : localReadersStateArray) {
            skipOrYield(readerState, prevVersionIndex);
        }
    }

    /**
     * Yield while the Reader is still reading.
     *
     * @param readerState The state of the Reader
     */
    private final void skipOrYield(final State readerState, final int localVersionIndex) {
        if (readerState == null) {
            return;
        }

        if (localVersionIndex == VERSION0) {
            while (readerState.v0State == STATE_READING) {
                Thread.yield();
            }
        } else {
            while (readerState.v1State == STATE_READING) {
                Thread.yield();
            }
        }
    }

    /**
     * A "Read" operation that returns an element.
     *
     * Performance: 1 ThreadLocal.get() + 2 Atomic.get() + 2 Atomic.set()
     *
     * @return Returns the value to which the specified key is mapped, or
     * {@code null} if this map contains no mapping for the key.
     */
    public boolean contains(E elem) {
        ReadersEntry localReadersEntry = entry.get();
        // Initialize a new Reader-state for this thread if needed         
        if (localReadersEntry == null) {
            localReadersEntry = addState();
        }
        boolean retValue;
        long localVersionIndex = versionIndex.get()%2;
        try {
            // Set the current Reader's state to READING
            if (localVersionIndex == VERSION0) {
                localReadersEntry.st.v0State = STATE_READING;
            } else {
                localReadersEntry.st.v1State = STATE_READING;
            }

            // Read the up-to-date value of leftRight.
            // Order is important: The leftRight value can only be read _after_
            // the Reader's state has been set.         
            if (leftRight.get() == READS_ON_LEFT) {
                retValue = leftTree.contains(elem);
            } else {
                retValue = rightTree.contains(elem);
            }
        } finally {
            // In the extreme event that TreeSet.contains() throws an exception, 
            // we want to make sure that no Writer is left hanging.
            if (localVersionIndex == VERSION0) {
                localReadersEntry.st.v0State = STATE_NOT_READING;
            } else {
                localReadersEntry.st.v1State = STATE_NOT_READING;
            }
        }
        return retValue;
    }

    
    /**
     * A "Read" operation that returns an element.
     * Similar to contains() but first try optimistic approach
     *
     * Performance: TODO 
     *
     * @return Returns the value to which the specified key is mapped, or
     * {@code null} if this map contains no mapping for the key.
     */
    public boolean optimisticContains(E elem) {
        boolean retValue;
        long localVersionIndex = versionIndex.get();
        int localLeftRight = leftRight.get();
        if (localLeftRight == READS_ON_LEFT) {
            retValue = leftTree.contains(elem);
        } else {
            retValue = rightTree.contains(elem);
        }
        
        if (retValue) {
            return true;
        }

        // We add a fence here to prevent the contains() above from
        // moving below the versionIndex.get()
        UNSAFE.loadFence();
        final long lVersionIndex = versionIndex.get();
        if (lVersionIndex == localVersionIndex) {
            return false;
        }

        ReadersEntry localReadersEntry = entry.get();
        // Initialize a new Reader-state for this thread if needed         
        if (localReadersEntry == null) {
            localReadersEntry = addState();
        }
        localVersionIndex = lVersionIndex % 2;
        try {
            // Set the current Reader's state to READING
            if (localVersionIndex == VERSION0) {
                localReadersEntry.st.v0State = STATE_READING;
            } else {
                localReadersEntry.st.v1State = STATE_READING;
            }

            // Read the up-to-date value of leftRight.
            // Order is important: The leftRight value can only be read _after_
            // the Reader's state has been set.         
            if (leftRight.get() == READS_ON_LEFT) {
                retValue = leftTree.contains(elem);
            } else {
                retValue = rightTree.contains(elem);
            }
        } finally {
            // In the extreme event that TreeSet.contains() throws an exception, 
            // we want to make sure that no Writer is left hanging.
            if (localVersionIndex == VERSION0) {
                localReadersEntry.st.v0State = STATE_NOT_READING;
            } else {
                localReadersEntry.st.v1State = STATE_NOT_READING;
            }
        }
        return retValue;
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
        int localLeftRight = leftRight.get();
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
    
    // Unsafe mechanics
    private static final sun.misc.Unsafe UNSAFE;
    static {
        try {
            Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            UNSAFE = (sun.misc.Unsafe) f.get(null);
        } catch (Exception e) {
            System.out.println("In Eclipse, add -Xbootclasspath/a:./bin/ to fix this exception\n");
            throw new Error(e);
        }
    }    
    
}
