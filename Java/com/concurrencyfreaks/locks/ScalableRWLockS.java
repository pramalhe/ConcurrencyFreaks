/******************************************************************************
 * Copyright (c) 2012-2013, Pedro Ramalhete, Andreia Correia
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
package com.concurrencyfreaks.locks;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicIntegerArray;
import java.util.concurrent.atomic.AtomicLong;


/**
 * A Read-Write Lock that is scalable with the number of threads doing Read.
 * The 'S' comes from the fact that it uses static variables to allow multiple 
 * instances to share the same thread-id.
 * Uses Andreia's state-machine for the readers, thus having only two states.
 * This is the simplest of all the algorithms for Scalable RW-Locks.
 * <p>
 * Advantages: <ul>
 * <li> Implements {@code java.util.concurrent.locks.ReadWriteLock}  
 * <li> When there are very few Writes, the performance scales with the 
 *      number of Reader threads 
 * </ul>
 * <p>
 * Disadvantages: <ul>
 * <li> Number of concurrent threads is limited to 64 (MAX_NUM_THREADS)
 * <li> Not Reentrant
 * <li> Needs to call initialization/cleanup functions per thread
 * <li> High memory footprint per instance
 * <li> Has Writer-Preference
 * <li> Does not support lockInterruptibly()
 * <li> Does not support newCondition()
 * </ul>
 * <p>
 * TODO: timed trylocks
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class ScalableRWLockS implements ReadWriteLock, java.io.Serializable {

    private static final long serialVersionUID = 9161245893848282286L;
    
    // CACHE_LINE=128/sizeof(int)=4 -> 32
    private final static int CACHE_PADD             = 32;
    // Definition of an invalid thread-id (must be negative)
    private final static int SRWL_INVALID_TID       = -1;
    private final static int SRWL_STATE_NOT_READING = 0;
    private final static int SRWL_STATE_READING     = 1;
    // Maximum number of concurrent threads at any given time
    private final static int MAX_NUM_THREADS        = 64;  

    // Instance variables
    // Stores the state of the Reader lock for each thread
    private transient final AtomicIntegerArray readersState;
    // The thread-id of the Writer currently holding the lock in write-mode
    private transient final AtomicLong writerOwner;
    private final InnerReadLock readerLock;
    private final InnerWriteLock writerLock;
    
    // Static variables shared by all instances
    private transient static final ThreadLocal<Integer> tidx = new ThreadLocal<Integer>();
    private transient static final ReentrantLock mutex = new ReentrantLock();           
    private transient static final AtomicInteger highestAssigned = new AtomicInteger(-1);
    private transient static final long[] assignedThreads = new long[MAX_NUM_THREADS]; // Protected by mutex
    
    
	static {
	    // Static constructor
        for (int i = 0; i < MAX_NUM_THREADS; i++) {
            assignedThreads[i] = SRWL_INVALID_TID; 
        }
	}
	
	/**
	 * Read-only lock
	 */
	final class InnerReadLock implements Lock {
        public void lock() { sharedLock(); }
        public void unlock() { sharedUnlock(); }
        public boolean tryLock() { return sharedTryLock(); }
        public boolean tryLock(long time, TimeUnit unit)
                throws InterruptedException {
            // TODO Auto-generated method stub
            return false;
        }
        public void lockInterruptibly() throws InterruptedException {
            // Not supported
            throw new UnsupportedOperationException();                 
        }        
        public Condition newCondition() {
            // Not supported
            throw new UnsupportedOperationException();        
        }	    	    
	}
	
	
    /**
     * Write-only lock
     */
	final class InnerWriteLock implements Lock {
        public void lock() { exclusiveLock(); }
        public void unlock() { exclusiveUnlock(); }
        public boolean tryLock() { return exclusiveTryLock(); }
        public boolean tryLock(long time, TimeUnit unit)
                throws InterruptedException {
            // TODO Auto-generated method stub
            return false;
        }
        public void lockInterruptibly() throws InterruptedException {
            // Not supported
            throw new UnsupportedOperationException();            
        }        
        public Condition newCondition() {
            // Not supported
            throw new UnsupportedOperationException();    
        }               
    }	
	
	
    /**
     * Default constructor.
     */
    public ScalableRWLockS() {	      		
        // States of the Readers, one per thread
        readersState = new AtomicIntegerArray(MAX_NUM_THREADS*CACHE_PADD);
        for (int i = 0; i < MAX_NUM_THREADS*CACHE_PADD; i += CACHE_PADD) {
            readersState.set(i, SRWL_STATE_NOT_READING);
        }

        writerOwner = new AtomicLong(SRWL_INVALID_TID);     
        readerLock = new ScalableRWLockS.InnerReadLock();
        writerLock = new ScalableRWLockS.InnerWriteLock();
    }
    
    public Lock readLock() { return readerLock; }
    public Lock writeLock() { return writerLock; }    

    
    /**
     * Should be called once on each thread before using each instance of this lock.
     * 
     * <p>Yes, this has a mutex protecting it, but it will be called only once 
     * per thread throughout the program's execution.
     */
    public static void threadInit() {
        if (tidx.get() != null) return;
        
        if (highestAssigned.get() > MAX_NUM_THREADS-1) {
            // The user is running with more threads than MAX_NUM_THREADS, or 
            // forgot to call threadCleanup() at some point.
            throw new IllegalMonitorStateException();
        }
        
        mutex.lock();        
        for (int i = 0; i < MAX_NUM_THREADS; i++) {
            if (assignedThreads[i] == SRWL_INVALID_TID) { 
                assignedThreads[i] = Thread.currentThread().getId();
                tidx.set(i*CACHE_PADD);
                if (i+1 > highestAssigned.get()) highestAssigned.set(i+1);
                break;
            }
        }
        mutex.unlock();
    }
    
    
    /**
     * Must be called before the thread exits 
     */
    public static void threadCleanup() {
        if (tidx.get() == null) return;
        
        mutex.lock();
        assignedThreads[tidx.get()/CACHE_PADD] = SRWL_INVALID_TID;
        tidx.set(null);
        // Search the highest non-occupied entry and set the highestAssigned to it       
        for (int i = MAX_NUM_THREADS-1; i > 0; i--) {
            if (assignedThreads[i] != SRWL_INVALID_TID) {
                highestAssigned.set(i+1);      
                break;
            }
        }
        mutex.unlock();
    }
    
    
    /**
     * Acquires the read lock.
     *
     * <p>Acquires the read lock if the write lock is not held by
     * another thread and returns immediately.
     *
     * <p>If the write lock is held by another thread then
     * the current thread yields until the write lock is released.
     */    
    public void sharedLock() {
        Integer tidRef = tidx.get();
        // Initialize the thread-id for this thread if needed        
        if (tidRef == null || tidRef.intValue() == SRWL_INVALID_TID) {
            threadInit();
            tidRef = tidx.get();
        }
        final int localTid = tidRef.intValue();

        // The "optimistic" code path takes only two synchronized calls:
        // a set() on a cache line that should be held in exclusive mode 
        // by the current thread, and a get() on a cache line that is shared.
        while (true) {
            readersState.set(localTid, SRWL_STATE_READING);
            if (writerOwner.get() == SRWL_INVALID_TID) {
                // Acquired lock in read-only mode
                return;
            } else {
                // A Writer has acquired the lock, must reset to 0 and wait
                readersState.set(localTid, SRWL_STATE_NOT_READING);
                // Some (other) thread is holding the write-lock, we must wait
                while (writerOwner.get() != SRWL_INVALID_TID) {
                   Thread.yield();
                }
            }
        }        
    }    
    
    
    /**
     * Attempts to release the read lock.
     *
     * If the current thread is not the holder of this lock then {@link
     * IllegalMonitorStateException} is thrown.
     *
     * @throws IllegalMonitorStateException if the current thread does not
     * hold this lock.
     */
    public void sharedUnlock() {
        final int localTid = tidx.get();   
        if (readersState.get(localTid) == SRWL_STATE_NOT_READING) {
            // ERROR: tried to unlock a non read-locked lock
            throw new IllegalMonitorStateException();
        } else {
            readersState.set(localTid, SRWL_STATE_NOT_READING);
            return;
        }
    }

    
    /**
     * Acquires the write lock.
     *
     * <p>Acquires the write lock if neither the read nor write lock are held 
     * by another thread and returns immediately.
     *
     * <p>If the lock is held by another thread then the current
     * thread becomes disabled for thread scheduling purposes and
     * lies dormant until the write lock has been acquired.
     */
    public void exclusiveLock() {
        final long tidSelf = Thread.currentThread().getId();
        
        // Try to acquire the lock in write-mode 
        while (!writerOwner.compareAndSet(SRWL_INVALID_TID, tidSelf)) { 
            Thread.yield();
        }
        
        // Write-Lock was acquired, now wait for any running Readers to finish
        for (int i = 0; i < highestAssigned.get()*CACHE_PADD; i += CACHE_PADD) {
            while (readersState.get(i) == SRWL_STATE_READING) {
                Thread.yield();
            }
        }
    }
    
    
    /**
     * Attempts to release this lock.
     *
     * If the current thread is not the holder of this lock then {@link
     * IllegalMonitorStateException} is thrown.
     *
     * @throws IllegalMonitorStateException if the current thread does not
     * hold this lock.
     */
    public void exclusiveUnlock() {
        final long tidSelf = Thread.currentThread().getId();
        if (writerOwner.get() != tidSelf) {
            // ERROR: tried to unlock a non write-locked lock
            throw new IllegalMonitorStateException();
        }
        writerOwner.set(SRWL_INVALID_TID);        
    }
    
    
    /**
     * Acquires the read lock only if the write lock is not held by
     * another thread at the time of invocation.
     *
     * <p>Acquires the read lock if the write lock is not held by another 
     * thread and returns immediately with the value {@code true}. 
     *
     * <p>If the write lock is held by another thread then
     * this method will return immediately with the value
     * {@code false}.
     *
     * @return {@code true} if the read lock was acquired
     */
    public boolean sharedTryLock() {
        Integer tidRef = tidx.get();
        // Initialize the thread-id for this thread if needed        
        if (tidRef == null || tidRef.intValue() == SRWL_INVALID_TID) {
            threadInit();
            tidRef = tidx.get();
        }
        final int localTid = tidRef.intValue();

        readersState.set(localTid, SRWL_STATE_READING);
        if (writerOwner.get() == SRWL_INVALID_TID) {
            // Acquired lock in read-only mode
            return true;
        } else {
            // A Writer has acquired the lock, must reset to "not reading" and lock has failed
            readersState.set(localTid, SRWL_STATE_NOT_READING);
            return false;
        }
    }
    
    
    /**
     * Acquires the write lock only if it is not held by another thread
     * at the time of invocation.
     *
     * <p>Acquires the write lock if neither the read nor write lock are held 
     * by another thread and returns immediately with the value {@code true}.
     *
     * <p>If the lock is held by another thread then this method
     * will return immediately with the value {@code false}.
     *
     * @return {@code true} if the lock was free and was acquired
     * by the current thread and {@code false} otherwise.
     */
    public boolean exclusiveTryLock() {
        final long tidSelf = Thread.currentThread().getId();        
        // Try to acquire the lock in write-mode and if it fails then return immediately 
        if (!writerOwner.compareAndSet(SRWL_INVALID_TID, tidSelf)) {
            return false; 
        }

        // Write-Lock was acquired, now check if there are Readers running
        for (int i = 0; i < highestAssigned.get()*CACHE_PADD; i += CACHE_PADD) {
            if (readersState.get(i) == SRWL_STATE_READING) {
                // There are Readers so undo the write-lock and return false
                writerOwner.set(SRWL_INVALID_TID);
                return false;
            }
        }
        
        return true;
    }
    
}
