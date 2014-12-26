/******************************************************************************
 * Copyright (c) 2014, Pedro Ramalhete, Andreia Correia
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
package com.concurrencyfreaks.readindicators;

import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

public class RIArrayCLQFinalizers implements ReadIndicator {

    // Reader states
    private static final int STATE_NOT_READING = 0;
    private static final int STATE_READING     = 1;

    /**
     * We use this as a "special" marking: readersStateArrayRef
     * will point to it when the array of Reader's states needs to be rebuild
     * from the ConcurrentLinkedQueue.
     */
    private static final AtomicInteger[] dummyArray = new AtomicInteger[0];    
    
    /**
     * List of Reader's states that the Writer will scan when attempting to
     * acquire the lock in write-mode
     */
    private final ConcurrentLinkedQueue<AtomicInteger> readersStateList;

    /**
     * Thread-local reference to the current thread's ReadersEntry instance.
     * It's from this instance that the current Reader thread is able
     * to determine where to store its own state for that particular thread.
     */
    private final ThreadLocal<ReadersEntry> entry;
    
    /**
     * Shortcut to the reader's states so that we don't have to walk the 
     * ConcurrentLinkedQueue on every exclusiveLock().
     */
    private final AtomicReference<AtomicInteger[]> readersStateArrayRef;

    
    RIArrayCLQFinalizers() {
        readersStateList = new ConcurrentLinkedQueue<AtomicInteger>();
        entry = new ThreadLocal<ReadersEntry>();        
        readersStateArrayRef = new AtomicReference<AtomicInteger[]>(null);
    }
    
    @Override
    public void arrive() {
        ReadersEntry localEntry = entry.get();
        // Initialize a new Reader-state for this thread if needed         
        if (localEntry == null) {
            localEntry = addState();      
        }
        localEntry.state.set(STATE_READING);
    }
    
    @Override
    public void depart() {
        entry.get().state.set(STATE_NOT_READING);
    }
    
    @Override
    public boolean isEmpty() {
        // We can only do this after the stampedLock has been acquired
        AtomicInteger[] localReadersStateArray = readersStateArrayRef.get();
        if (localReadersStateArray == null) {
            // Set to dummyArray before scanning the readersStateList to impose
            // a linearizability condition
            readersStateArrayRef.set(dummyArray);
            // Copy readersStateList to an array
            localReadersStateArray = readersStateList.toArray(new AtomicInteger[readersStateList.size()]);
            readersStateArrayRef.compareAndSet(dummyArray, localReadersStateArray);            
        }         
        
        // Scan the array of Reader states
        for (AtomicInteger readerState : localReadersStateArray) {
            while (readerState != null && readerState.get() == STATE_READING) {
                return false;
            }
        }

        return true;
    }
    
    /**
     * Inner class that makes use of finalize() to remove the Reader's state 
     * from the ConcurrentLinkedQueue {@code readersStateList}
     */
    final class ReadersEntry {
        public final AtomicInteger state;
        public boolean isStampLocked = false;       
        public ReadersEntry(AtomicInteger state) { 
            this.state = state;         
        }
        protected void finalize() throws Throwable {
            removeState(state);
            super.finalize();
        }
    }    

    /**
     * This function should be called only from ReadersEntry.finalize()
     * 
     * @param state The reader's state that we wish to remove from the ConcurrentLinkedQueue
     */
    protected void removeState(AtomicInteger state) {        
        readersStateList.remove(state);
        // Setting the readersStateArrayRef to null will make the Writer
        // rebuild the array from the CLQ.
        readersStateArrayRef.set(null);        
        // Paranoia: just in case someone forgot to call sharedUnlock()
        // and there is a Writer waiting on that state.
        state.set(STATE_NOT_READING);
    }

    
    /**
     * Creates a new ReadersEntry instance for the current thread and
     * its associated AtomicInteger to store the state of the Reader
     * 
     * @return Returns a reference to the newly created instance of 
     * {@code ReadersEntry}
     */
    private ReadersEntry addState() {
        final AtomicInteger state = new AtomicInteger(STATE_NOT_READING);
        final ReadersEntry newEntry = new ReadersEntry(state); 
        entry.set(newEntry);
        readersStateList.add(state);
        // Setting the readersStateArrayRef to null will make the Writer
        // rebuild the array from the CLQ.
        readersStateArrayRef.set(null);
        return newEntry;
    }
    
}
