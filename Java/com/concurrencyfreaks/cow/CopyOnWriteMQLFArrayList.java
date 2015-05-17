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

import java.util.ArrayList;
import java.util.Collection;

/**
 * <h1> ArrayList with a Copy-On-Write with Mutation Queue (Lock-Free) </h1>
 * 
 * TODO:
 * 
 * This is a class imilar to java.util.concurent.CopyOnWriteArrayList but uses 
 * COW+MutationQ instead of COW+Lock
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 *
 */

public class CopyOnWriteMQLFArrayList<E> extends COWMutationQLF<E, Collection<? extends E>, Integer, CopyOnWriteMQLFArrayList.CopyableArrayList<E>>{
    
    public static class CopyableArrayList<E> extends ArrayList<E> implements IShallowCopy {
        private static final long serialVersionUID = 1987605188174875967L;
        public CopyableArrayList() { super(); }
        public CopyableArrayList(CopyableArrayList<E> instance) { super(instance); }
        @Override
        public Object copyOf() {
            return new CopyableArrayList<E>(this);
        }
    }
    
    public CopyOnWriteMQLFArrayList() {
        super(new CopyableArrayList<E>());
    }

    public int size() {
        return combinedRef.instance.size();
    }
    
    public boolean isEmpty() {
        return combinedRef.instance.isEmpty();
    }
    
    public boolean contains(Object o) {
        return combinedRef.instance.contains(o);
    }
    
    public int indexOf(Object o) {
        return combinedRef.instance.indexOf(o);
    }
    
    public int lastIndexOf(Object o) {
        return combinedRef.instance.lastIndexOf(o);
    }
    
    public Object[] toArray() {
        return combinedRef.instance.toArray();
    }
    
    public <T> T[] toArray(T a[]) {
        return combinedRef.instance.toArray(a);
    }
    
    public E get(int index) {
        return combinedRef.instance.get(index);
    }
    
    public E set(int index, E element) {
        return applyMutation(element, null, index, (E _element, Collection<? extends E> _nop, Integer _index, ArrayList<E> list) -> list.set(_index, _element));
    }
        
    public boolean add(E e) {
        return applyMutation(e, null, null, (E _e, Collection<? extends E> _nop1, Integer _nop2, ArrayList<E> list) -> list.add(_e));
    }
    
    public void add(int index, E element) {
        applyMutation(element, null, index, (E _element, Collection<? extends E> _nop1, Integer _index, ArrayList<E> list) -> { list.add(_index, _element); return null; });
    }
    
    public E remove(int index) {
        return applyMutation(null, null, index, (E _nop1, Collection<? extends E> _nop2, Integer _index, ArrayList<E> list) -> list.remove((int)_index));
    }

    public boolean remove(Object o) {
        return applyMutation((E)o, null, null, (E _element, Collection<? extends E> _nop2, Integer _nop3, ArrayList<E> list) -> list.remove(_element));
    }
    
    public void clear() {
        applyMutation(null, null, null, (E _nop1, Collection<? extends E> _nop2, Integer _nop3, ArrayList<E> list) -> { list.clear(); return null; });
    }
    
    public boolean addAll(Collection<? extends E> c) {
        return applyMutation(null, c, null, (E _nop1, Collection<? extends E> _c, Integer _nop3, ArrayList<E> list) -> list.addAll(_c));
    }
    
    
    // TODO:
    // public boolean removeAll(Collection<?> c) {
    // public boolean retainAll(Collection<?> c) {
    // others...
    
}
