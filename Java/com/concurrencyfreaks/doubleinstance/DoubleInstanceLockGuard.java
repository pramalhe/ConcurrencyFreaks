package com.concurrencyfreaks.doubleinstance;
/******************************************************************************
 * Copyright (c) 2013-2015, Pedro Ramalhete and Andreia Correia
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

import java.util.concurrent.locks.ReentrantLock;
import com.concurrencyfreaks.locks.LongAdderStampedRWLock;


/** <h1>Double Instance Lock</h1>
 * A Thread-safe Guard pattern that has Lock-Free properties for Reads.
 * <p>
 * Although it uses three locks (one mutex and two reader-writer locks) this
 * technique is actually lock-free for read operations.
 * It was inspired by the algorithm described in the paper "Left-Right: A 
 * Concurrency Control Technique with Wait-Free Population Oblivious Reads".
 * <p>
 * This class allows easy usage of the Double-Instance-Lock pattern as if it
 * was a Reader-Writer Lock (but Read operations will not be "Blocking"). 
 * The main disadvantage compared with a "normal" Reader-Writer Lock is that 
 * this needs to be associated to the object (or pair of objects) that it 
 * is protecting. 
 * <p>
 * With this pattern, you can provide concurrent access to any (duplicable) 
 * object or data structure with Linearizability.
 * <p>
 * Example:
 * - Initialize with two equal instances of whatever it is you want to protect:
 *   guard = new DoubleInstanceLock<UserClass>(new UserClass(), new UserClass());
 *   
 * - To access in read-only mode do something like this:
 *      final UserClass userInstance = guard.readLock();
 *      userInstance.readDataFromObject();        
 *      guard.readUnlock();
 * 
 * - To access in write-modify mode:
 *      UserClass userInstance = guard.writeLock();
 *      userInstance.modifyThisObject();
 *      userInstance = guard.writeToggle();
 *      userInstance.modifyThisObject();
 *      guard.writeUnlock();
 *      
 * The exact same operations must be done on the instance before and after 
 * guard.writeToggle(), which means that those operations should have no
 * "side-effects" outside of the instance.
 * <p>
 * You can also pass null to both instances in the constructor for greater 
 * flexibility in how you use this technique.
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class DoubleInstanceLockGuard<E> implements java.io.Serializable {
	
    private static final long serialVersionUID = 3368775051824403132L;
    
    // This mutual exclusion lock guarantees that only one Writer at a time
    // is accessing.
    private transient final ReentrantLock writersMutex;    
    
    // Two rw-locks, one to protect instance1 and another to protect instance2    
    private transient final LongAdderStampedRWLock rwlock1;   
    private transient final LongAdderStampedRWLock rwlock2;
    
    private final E instance1;
    private final E instance2;
       
    private transient final ThreadLocal<LongAdderStampedRWLock> heldLock;

    
    /**
     * Default constructor.
     * Must pass the two instances that will be used on the Double Instance 
     * Lock pattern.
     */
    public DoubleInstanceLockGuard(E instance1, E instance2) {
        this.instance1 = instance1;
        this.instance2 = instance2;       
        writersMutex = new ReentrantLock();
        rwlock1 = new LongAdderStampedRWLock();
        rwlock2 = new LongAdderStampedRWLock();
        heldLock = new ThreadLocal<LongAdderStampedRWLock>();
    }
 
             
    /**
     * 
     * @return
     */
    public E readLock() {       
        while (true) {
            if (rwlock1.sharedTryLock()) {
                heldLock.set(rwlock1);
                return instance1;
            }
            if (rwlock2.sharedTryLock()) {
                heldLock.set(rwlock2);
                return instance2;
            }
        }
    }
    
    
    /**
     * 
     */
    public void readUnlock() {
        heldLock.get().sharedUnlock();
    }

    
    /**
     * 
     * @return the instance that is to be used in the first step, until 
     * writeToggle() is called
     */
    public E writeLock() {
    	writersMutex.lock();   	
    	rwlock2.exclusiveLock();
    	return instance1;
    }
    
    
    /**
     * 
     * @return the instance that is to be used in the second step, until
     * writeUnlock() is called
     */
    public E writeToggle() {
        rwlock2.exclusiveUnlock();
        rwlock1.exclusiveLock();
        return instance2;
    }
    
    
    /**
     * 
     */
    public void writeUnlock() {
        rwlock1.exclusiveUnlock();
        writersMutex.unlock();
    }

}
