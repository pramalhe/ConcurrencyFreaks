/******************************************************************************
 * Copyright (c) 2012-2014, Pedro Ramalhete, Andreia Correia
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


package com.concurrencyfreaks.papers.LeftRight;

import java.util.Comparator;
import java.util.TreeSet;
import com.concurrencyfreaks.locks.ScalableRWLock;



/**
 * ScalableRWLockTreeSet
 * A TreeSet that uses a Scalable Read-Write Lock to protect its methods.
 * 
 * Advantages:
 * - Faster than synchronized TreeSet 
 * 
 * Disadvantages:
 * - Blocking
 * 
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class ScalableRWLockTreeSet<E> 
    implements Cloneable
{
    final private TreeSet<E> treeSet;
    final private ScalableRWLock srwlock = new ScalableRWLock();
    
    public ScalableRWLockTreeSet() {
        treeSet = new TreeSet<E>();
    }
    
    public ScalableRWLockTreeSet(Comparator<? super E> comparator) {
        treeSet = new TreeSet<E>(comparator);
    }
 

    public void clear() {
        srwlock.exclusiveLock();
        try {
            treeSet.clear();
        } finally {
            srwlock.exclusiveUnlock();
        }
    }

    public Object clone() {
        srwlock.sharedLock();
        try {
            return treeSet.clone();
        } finally {
            srwlock.sharedUnlock();
        }
    }
    
    public Comparator<? super E> comparator() {
        srwlock.sharedLock();
        try {
            return treeSet.comparator();
        } finally {
            srwlock.sharedUnlock();
        }        
    }
    
    public boolean containsKey(Object key) {
        srwlock.sharedLock();
        try {
            return treeSet.contains(key);
        } finally {
            srwlock.sharedUnlock();
        }        
    }
        
    public E first() {
        srwlock.sharedLock();
        try {
            return treeSet.first();
        } finally {
            srwlock.sharedUnlock();
        }          
    }
    
    public E higher(E key) {
        srwlock.sharedLock();
        try {
            return treeSet.higher(key);
        } finally {
            srwlock.sharedUnlock();
        }        
    }
    
    public boolean isEmpty() {
        srwlock.sharedLock();
        try {
            return treeSet.isEmpty();
        } finally {
            srwlock.sharedUnlock();
        }   
    }
    
    public E lowerKey(E key) {
        srwlock.sharedLock();
        try {
            return treeSet.lower(key);
        } finally {
            srwlock.sharedUnlock();
        }
    }   
    
    public boolean add(E elem) {
        srwlock.exclusiveLock();
        try {
            return treeSet.add(elem);
        } finally {
           srwlock.exclusiveUnlock();
        }
    }
        
    public boolean remove(E elem) {
        srwlock.exclusiveLock();
        try {
            return treeSet.remove(elem);
        } finally {
            srwlock.exclusiveUnlock();
        }        
    }
    
    public int size() {
        srwlock.sharedLock();
        try {
            return treeSet.size();
        } finally {
            srwlock.sharedUnlock();
        }        
    }    
    
    public String toString() {
        srwlock.sharedLock();
        try {
            return treeSet.toString();
        } finally {
        srwlock.sharedUnlock();
        }        
    }   
}
