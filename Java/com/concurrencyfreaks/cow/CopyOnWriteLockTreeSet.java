/******************************************************************************
 * Copyright (c) 2015, Pedro Ramalhete, Andreia Correia
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
package com.concurrencyfreaks.cow;

import java.util.Collection;
import java.util.TreeSet;
import java.util.concurrent.locks.ReentrantLock;

/**
 * <h1> TreeSet with a Copy-On-Write with a Lock for mutations </h1>
 * 
 * Uses COW+Lock
 * 
 * TODO:
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 *
 */
public class CopyOnWriteLockTreeSet<E> implements ITreeSet<E> {
    
    private volatile TreeSet<E> treeSet = new TreeSet<E>();
    
    private final ReentrantLock lock = new ReentrantLock();
    
    /**
     * 
     * Progress Condition: Wait-Free Population Oblivious
     * 
     * @param key
     * @return
     */
    public boolean contains(Object key) {
        return treeSet.contains(key);
    }
    
    
    public boolean add(E key) {
        lock.lock();
        try {
            final TreeSet<E> newTreeSet = new TreeSet<E>(treeSet);
            final boolean retValue = newTreeSet.add(key);
            treeSet = newTreeSet;
            return retValue;
        } finally {
            lock.unlock();
        }
    }
    
    
    public boolean remove(E key) {
        lock.lock();
        try {
            final TreeSet<E> newTreeSet = new TreeSet<E>(treeSet);
            final boolean retValue = newTreeSet.remove(key);
            treeSet = newTreeSet;
            return retValue;
        } finally {
            lock.unlock();
        }
    }
    
    
    public boolean addAll(Collection<? extends E> c) {
        lock.lock();
        try {
            final TreeSet<E> newTreeSet = new TreeSet<E>(treeSet);
            final boolean retValue = newTreeSet.addAll(c);
            treeSet = newTreeSet;
            return retValue;
        } finally {
            lock.unlock();
        }
    }
    
}
