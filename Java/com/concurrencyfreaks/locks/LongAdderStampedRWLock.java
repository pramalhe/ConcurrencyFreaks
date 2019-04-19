/******************************************************************************
 * Copyright (c) 2013, Pedro Ramalhete, Andreia Correia
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
package com.concurrencyfreaks.scalablerwlock;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.LongAdder;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.StampedLock;



/** <h1> LongAdder with Stamped Read-Write Lock </h1>
 * A Read-Write Lock that is scalable with the number of threads doing Read.
 * <p>
 * More info and performance plots can be seen here:
 * <a href="http://concurrencyfreaks.com/2013/09/combining-stampedlock-and-longadder-to.html">
 * Combining the StampedLock and LongAdder to make a new RW-Lock</a>
 * <p>
 * Uses a two-state-machine for the Readers, and averages two synchronized 
 * operations. <br>
 * Although this mechanism was independently designed and implemented by the
 * authors, the idea is very similar to the algorithm C-RW-WP described in this
 * paper: <a href="http://blogs.oracle.com/dave/resource/ppopp13-dice-NUMAAwareRWLocks.pdf">
 * NUMA-Aware Reader-Writer locks</a> 
 * <br>
 * Relative to the paper, there are two differences: The threads have no 
 * particular order, which means this implementation is <b>not</b> NUMA-aware;
 * Threads attempting a read-lock are added to a counter of Readers.
 * <p>
 * The counter of Readers is composed of two LongAdder variables that get 
 * incremented, one when a Reader arrives and the other when the Reader
 * departs. We do this instead of increment()/decrement() a single LongAdder
 * because the LongAdder is not Sequentially Consistent for sum() when 
 * decrement() and increment() are done, but if only increment() is done then
 * it is sequentially consistent.
 * <p>
 * Advantages: <ul>
 * <li> Implements {@code java.util.concurrent.locks.ReadWriteLock} 
 * <li> When there are very few Writes, the performance scales with the 
 *      number of Reader threads
 * <li> Supports optimistic Reads like the StampedLock 
 * <li> Fairness proprieties are similar to StampedLock      
 * <li> No need to call initialization/cleanup functions per thread
 * <li> No limitation on the number of concurrent threads
 * <li> Consumes at most O(N_cores) memory, which is less than the ScalableStampedRWLock
 * </ul>
 * <p>
 * Disadvantages: <ul>
 * <li> Not Reentrant
 * <li> Does not support {@code lockInterruptibly()}
 * <li> Does not support {@code newCondition()}
 * </ul>
 * <p>
 * For scenarios with few writes, the average case for {@code sharedLock()} is
 * two synchronized calls: an {@code AtomicInteger.set()} on a cache line that 
 * is held in exclusive mode by the core where the current thread is running, 
 * and an {@code AtomicLong.get()} on a shared cache line.<br>
 * This means that when doing several sequential calls of sharedLock()/unlock() 
 * on the same instance, the performance penalty will be small because the 
 * accessed variables will most likely be in L1/L2 cache.  
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class LongAdderStampedRWLock implements ReadWriteLock, java.io.Serializable {

    private static final long serialVersionUID = -1275456836855114993L;
       
    /**
     * We have to use two LongAdder because LongAdder is not sequentially 
     * consistent when both increment() and decrement() are used.
     * We had used this idea before, but you can see it also in the 
     * ingress/egress counters from "NUMA-Aware Reader Writer Locks" paper.
     */
    private transient final LongAdder readersIngress;
    private transient final LongAdder readersEgress;
    
    /** Stamped lock that is used mostly as writer-lock */   
    private transient final StampedLock stampedLock;       
        
    /** The lock returned by method {@link LongAdderStampedRWLock#rwLock} */
    private final InnerReadLock readerLock;
    
    /** The lock returned by method {@link LongAdderStampedRWLock#writeLock} */
    private final InnerWriteLock writerLock;
    
    /** Reference to the current thread's ReadersEntry instance */
    private transient final ThreadLocal<ReadersEntry> entry;
	
    /**
     * Inner class that holds state for the Readers.
     * Each Reader will have an instance of this class which is used for the 
     * sole purpose of having a boolean that the ThreadLocal can point to.
     * This boolean is used to indicate whether or not the StampedLock has been
     * acquired (in read-only mode) so that the sharedUnlock() function can
     * determine if it needs to unlock the StampedLock or not.
     */
    @sun.misc.Contended
    static final class ReadersEntry {
        public boolean isStampLocked = false;
        public ReadersEntry(boolean value) { isStampLocked = value; }
    }  

    
    /**
     * Read-only lock
     */
    final class InnerReadLock implements Lock {
        public void lock() { sharedLock(); }
        public void unlock() { sharedUnlock(); }        
        public boolean tryLock() { return sharedTryLock(); }
        public boolean tryLock(long timeout, TimeUnit unit)
                throws InterruptedException {
            if (Thread.interrupted()) throw new InterruptedException();            
            return sharedTryLockNanos(unit.toNanos(timeout));
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
        public boolean tryLock(long timeout, TimeUnit unit)
                throws InterruptedException {
            if (Thread.interrupted()) throw new InterruptedException();
            return exclusiveTryLockNanos(unit.toNanos(timeout));
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
     * Default constructor
     */
    public LongAdderStampedRWLock() {	      		
        readersIngress = new LongAdder();
        readersEgress = new LongAdder();
        stampedLock = new StampedLock();        
        entry = new ThreadLocal<ReadersEntry>();
        readerLock = new LongAdderStampedRWLock.InnerReadLock();
        writerLock = new LongAdderStampedRWLock.InnerWriteLock();
    }
    
    public Lock readLock() { return readerLock; }
    public Lock writeLock() { return writerLock; }    
    

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
        while (true) {
            readersIngress.increment();
            if (!stampedLock.isWriteLocked()) {
                // Acquired lock in read-only mode
                return;
            } else {
                // Rollback logical counter to avoid blocking a Writer
                readersEgress.increment();
                // If there is a Writer, we go for the StampedLock.readlock()
                if (stampedLock.isWriteLocked()) {                    
                    stampedLock.asReadLock().lock();
                    final ReadersEntry localEntry = entry.get();
                    if (localEntry == null) {
                        // Yes we allocate memory, but only once per thread, 
                        // and we're stuck waiting for the Writer anyways.
                        entry.set(new ReadersEntry(true));
                    } else {
                        localEntry.isStampLocked = true;
                    }
                    return;
                }
            }
        }        
    }    
    
    
    /**
     * Attempts to release the read lock.
     */    
    public void sharedUnlock() {
        final ReadersEntry localEntry = entry.get();
        if (localEntry != null) {
            if (localEntry.isStampLocked) {
                localEntry.isStampLocked = false;
                stampedLock.asReadLock().unlock();
                return;
            } 
        } 
        readersEgress.increment();
        return;
    }
    
    
    /**
     * Acquires the write lock.
     *
     * <p>Acquires the write lock if neither the read nor write lock
     * are held by another thread and returns immediately.
     *
     * <p>If the lock is held by another thread, then the current
     * thread yields and lies dormant until the write lock has been acquired.
     */
    public void exclusiveLock() {
        // Try to acquire the stampedLock in write-mode 
        stampedLock.writeLock(); 
        while (true) {
            // Order is _very_ important here
            final long egressSum = readersEgress.sum();
            final long ingressSum = readersIngress.sum();
            if (egressSum == ingressSum) return;
            // TODO: Do something smarter, like spin for a while and then yield()
            Thread.yield();
        }
    }
    
        
    /**
     * Attempts to release the write lock.
     * 
     * If the current thread is not the holder of this lock then {@link
     * IllegalMonitorStateException} is thrown.
     *
     * @throws IllegalMonitorStateException if the current thread does not
     * hold this lock.
     */    
    public void exclusiveUnlock() {
        if (!stampedLock.isWriteLocked()) {
            // ERROR: tried to unlock a non write-locked instance
            throw new IllegalMonitorStateException();
        }
        stampedLock.asWriteLock().unlock();
    }
        
    
   /**    
    * Acquires the read lock only if the write lock is not held by
    * another thread at the time of invocation.
    *
    * <p>Acquires the read lock if the write lock is not held by
    * another thread and returns immediately with the value
    * {@code true}. 
    *
    * <p>If the write lock is held by another thread then
    * this method will return immediately with the value
    * {@code false}.
    *
    * @return {@code true} if the read lock was acquired
    */
    public boolean sharedTryLock() {
        readersIngress.increment();
        if (!stampedLock.isWriteLocked()) {
            // Acquired lock in read-only mode
            return true;
        } else {
            // Lock can not be acquired right now. Rollback logical counter
            readersEgress.increment();
            return false;
        }         
    }
    
    
    /**
     * Acquires the read lock if the write lock is not held by
     * another thread within the given waiting time.
     *
     * <p>Acquires the read lock if the write lock is not held by
     * another thread and returns immediately with the value
     * {@code true}.
     *
     * <p>If the write lock is held by another thread then the
     * current thread yields execution until one of two things happens:
     * <ul>
     * <li>The read lock is acquired by the current thread; or
     * <li>The specified waiting time elapses.
     * </ul>
     *
     * <p>If the read lock is acquired then the value {@code true} is
     * returned.
     *
     * @param nanosTimeout the time to wait for the read lock in nanoseconds
     * @return {@code true} if the read lock was acquired
     */
    public boolean sharedTryLockNanos (long nanosTimeout) throws InterruptedException {
        readersIngress.increment();
        if (!stampedLock.isWriteLocked()) {
            // Acquired lock in read-only mode
            return true;
        } else {
            // Rollback logical counter to avoid blocking a Writer
            readersEgress.increment();
            // If there is a Writer, we go for the StampedLock.readlock()
            if (stampedLock.isWriteLocked()) {      
                if (stampedLock.asReadLock().tryLock(nanosTimeout, TimeUnit.NANOSECONDS)) {
                    final ReadersEntry localEntry = entry.get();
                    if (localEntry == null) {
                        entry.set(new ReadersEntry(true));
                    } else {
                        localEntry.isStampLocked = true;
                    }
                    return true;
                } // else
                return false;
            }
        }
        return false;        
    }
    
    
    /**
     * Acquires the write lock only if it is not held by another thread
     * at the time of invocation.
     *
     * <p>Acquires the write lock if the write lock is not
     * held by another thread and returns immediately with 
     * the value {@code true} if and only if no other thread is attempting a 
     * read lock. 
     *
     * <p>If the write lock is held by another thread then this method
     * will return immediately with the value {@code false}.
     *
     * @return {@code true} if the write lock was free and was acquired
     * by the current thread and {@code false} otherwise.
     */   
    public boolean exclusiveTryLock() {
        // Try to acquire the stampedLock in write-mode 
        if (stampedLock.tryWriteLock() == 0) {
            return false;
        }
        // Order is _very_ important here
        final long egressSum = readersEgress.sum();
        final long ingressSum = readersIngress.sum();
        if (egressSum == ingressSum) {
            return true;
        } // else
        stampedLock.asWriteLock().unlock();
        return false;
    }
    
    
    /**
     * Acquires the write lock if it is not held by another thread
     * within the given waiting time.
     *
     * <p>Acquires the write lock if the write lock is not
     * held by another thread and returns immediately with 
     * the value {@code true} if and only if no other thread is attempting a 
     * read lock, setting the write lock {@code reentrantWriterCount} 
     * to one. If another thread is attempting a read lock, this
     * function <b>may yield until the read lock is released</b>.
     * 
     * <p>If the write lock is held by another thread then the current
     * thread yields and lies dormant until one of two things happens:
     * <ul>
     * <li>The write lock is acquired by the current thread; or
     * <li>The specified waiting time elapses
     * </ul>
     * 
     * <p>If the read lock is held by another thread then the current
     * thread yields and lies dormant until the read lock is released.
     *
     * <p>If the write lock is acquired then the value {@code true} is
     * returned and the write lock {@code reentrantWriterCount} is set to one.
     *
     * <p>There is no guarantee that there is a maximum waiting time for
     * this method.
     *
     * @param nanosTimeout the time to wait for the write lock in nanoseconds
     *
     * @return {@code true} if the lock was free and was acquired
     * by the current thread, or the write lock was already held by the
     * current thread; and {@code false} if the waiting time
     * elapsed before the lock could be acquired.
     */    
    public boolean exclusiveTryLockNanos(long nanosTimeout) throws InterruptedException {
        final long lastTime = System.nanoTime();
        // Try to acquire the stampedLock in write-mode 
        if (stampedLock.tryWriteLock(nanosTimeout, TimeUnit.NANOSECONDS) == 0) {
            return false;
        }

        while (true) {
            // Order is _very_ important here
            final long egressSum = readersEgress.sum();
            final long ingressSum = readersIngress.sum();
            if (egressSum == ingressSum) return true;
            if (System.nanoTime() - lastTime < nanosTimeout) {
                Thread.yield();
            } else { 
                // Time has expired and there is still at least one Reader so give up
                stampedLock.asWriteLock().unlock();
                return false;
            }  
        }
    }


    /**
     * Returns a stamp that can later be validated, or zero
     * if exclusively locked.
     *
     * @return a stamp, or zero if exclusively locked
     */    
    public long tryOptimisticRead() {
        return stampedLock.tryOptimisticRead();
    }
    
    
    /**
     * Returns true if the lock has not been exclusively acquired
     * since issuance of the given stamp. Always returns false if the
     * stamp is zero. Always returns true if the stamp represents a
     * currently held lock. Invoking this method with a value not
     * obtained from {@link #tryOptimisticRead} has no defined effect 
     * or result.
     *
     * @return true if the lock has not been exclusively acquired
     * since issuance of the given stamp; else false
     */
    public boolean validate(long stamp) {
        return stampedLock.validate(stamp);
    }    
}
