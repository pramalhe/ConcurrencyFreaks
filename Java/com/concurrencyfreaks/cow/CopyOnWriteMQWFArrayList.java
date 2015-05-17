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
import java.util.Comparator;
import java.util.Map;
import java.util.NavigableMap;
import java.util.NavigableSet;
import java.util.Set;
import java.util.SortedMap;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.function.BiConsumer;
import java.util.function.BiFunction;


/**
 * <h1> TreeMap with a Copy-On-Write with Mutation Queue (Wait-Free) </h1>

 * TODO:
 * TODO: extend AbstractMap<K,V> implements ConcurrentMap<K,V>, Serializable
 * 
 * <h2>About views</h2>
 * Methods that provide views into the set are linearizable, i.e. provide a
 * snapshot consistent view of the set, but be aware that modifying the view
 * may change (one of the instances of) the TreeMap, thus breaking mutual exclusion
 * and causing undefined behavior.
 * 
 * Casting to/from Object kind of defeats the purpose of using Generics, but ohh well.
 * 
 * 
 * @see TreeMap
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 *
 */
public class CopyOnWriteMQWFArrayList<E> 
    extends COWMutationQWF<E, E, Integer, Object, Object, CopyOnWriteMQWFArrayList.CopyableArrayList<E>> { 
    
    public static class CopyableArrayList<E> extends ArrayList<E> implements IShallowCopy {
        private static final long serialVersionUID = -3657341719612175899L;
        public CopyableArrayList() { super(); }
        public CopyableArrayList(int initialCapacity) { super(initialCapacity); }
        public CopyableArrayList(Collection<? extends E> c) { super(c); }
        @Override
        public Object copyOf() {
            return new CopyableArrayList<E>(this);
        }
    }
        
    
    /*
     * Constructors
     */
    
    public CopyOnWriteMQWFArrayList() {
        super(new CopyableArrayList<E>());
    }

    public CopyOnWriteMQWFArrayList(int initialCapacity) {
        super(new CopyableArrayList<E>(initialCapacity));
    }

    public CopyOnWriteMQWFArrayList(Collection<? extends E> c) {
        super(new CopyableArrayList<E>(c));
    }

    
    
    /*
     * Read-only methods
     */
        
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
    
    public <T> T[] toArray(T[] a) {
        return combinedRef.instance.toArray(a);
    }
    
    public E get(int index) {
        return combinedRef.instance.get(index);
    }
    
    
    
    
    
    
    
    
    
    
    /*
     * Mutative methods
     */
    
    public void trimToSize() {
        applyMutation(null, null, null, null, null, (E _e1, E _e2, Integer _i, Object _o2, Object _o3, ArrayList<E> array) -> {
        array.trimToSize(); 
        return 0;
        });
    }
    
    public void ensureCapacity(int minCapacity) {
        applyMutation(null, null, minCapacity, null, null, (E _e1, E _e2, Integer _minCapacity, Object _o2, Object _o3, ArrayList<E> array) -> {
        array.ensureCapacity(_minCapacity); 
        return 0;
        });
    }
    
    public E set(int index, E element) {
        return applyMutation(element, null, index, null, null, (E _element, E _e2, Integer _index, Object _o2, Object _o3, ArrayList<E> array) -> 
        array.set(_index, _element)
        );        
    }

    public boolean add(E e) {
        return applyMutation(e, null, null, null, null, (E _e, E _e2, Integer _i, Object _o2, Object _o3, ArrayList<E> array) -> 
        array.add(_e)
        );                
    }
    
    public void add(int index, E element) {
        applyMutation(element, null, index, null, null, (E _element, E _e2, Integer _index, Object _o2, Object _o3, ArrayList<E> array) -> { 
        array.add(_index, _element);
        return 0;
        });                
    }
    
    public E remove(int index) {
        return applyMutation(null, null, index, null, null, (E _e, E _e2, Integer _index, Object _o2, Object _o3, ArrayList<E> array) -> 
        array.remove((int)_index)
        );                        
    }
    
    public boolean remove(Object o) {
        return applyMutation(null, null, null, o, null, (E _e, E _e2, Integer _index, Object _o, Object _o3, ArrayList<E> array) -> 
        array.remove(_o)
        );                        
    }    
    
    public void clear() {
        applyMutation(null, null, null, null, null, (E _element, E _e2, Integer _index, Object _o2, Object _o3, ArrayList<E> array) -> { 
        array.clear();
        return 0;
        });                        
    }
    
    public boolean addAll(Collection<? extends E> c) {
        return applyMutation(null, null, null, c, null, (E _e, E _e2, Integer _index, Object _c, Object _o3, ArrayList<E> array) -> 
        array.addAll((Collection<? extends E>)_c)
        );                                
    }
    
    public boolean addAll(int index, Collection<? extends E> c) {
        return applyMutation(null, null, index, c, null, (E _e, E _e2, Integer _index, Object _c, Object _o3, ArrayList<E> array) -> 
        array.addAll(_index, (Collection<? extends E>)_c)
        );                                        
    }
    
    public boolean removeAll(Collection<?> c) {
        return applyMutation(null, null, null, c, null, (E _e, E _e2, Integer _index, Object _c, Object _o3, ArrayList<E> array) -> 
        array.removeAll((Collection<? extends E>)_c)
        );                                        
    }
    
    public boolean retainAll(Collection<?> c) {
        return applyMutation(null, null, null, c, null, (E _e, E _e2, Integer _index, Object _c, Object _o3, ArrayList<E> array) -> 
        array.retainAll((Collection<? extends E>)_c)
        );        
    }
    
    public void sort(Comparator<? super E> c) {
        applyMutation(null, null, null, c, null, (E _element, E _e2, Integer _index, Object _c, Object _o3, ArrayList<E> array) -> { 
        array.sort((Comparator<? super E>)_c);
        return 0;
        });                                
    }
     
    
}
