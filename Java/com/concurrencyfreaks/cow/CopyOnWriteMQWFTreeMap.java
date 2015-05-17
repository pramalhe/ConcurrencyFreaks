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
public class CopyOnWriteMQWFTreeMap<K,V> 
    extends COWMutationQWF<K, V, V, Object, Object, CopyOnWriteMQWFTreeMap.CopyableTreeMap<K,V>> { 
    
    public static class CopyableTreeMap<K,V> extends TreeMap<K,V> implements IShallowCopy {
        private static final long serialVersionUID = -3657341719612175899L;
        public CopyableTreeMap() { super(); }
        public CopyableTreeMap(CopyableTreeMap<K,V> tree) { super(tree); }
        public CopyableTreeMap(Map<? extends K, ? extends V> tree) { super(tree); }
        public CopyableTreeMap(SortedMap<? extends K, ? extends V> tree) { super(tree); }
        public CopyableTreeMap(Comparator<? super K> comparator) { super(comparator); }
        @Override
        public Object copyOf() {
            return new CopyableTreeMap<K,V>(this);
        }
    }
    
    
    /*
     * Constructors
     */
    
    public CopyOnWriteMQWFTreeMap() {
        super(new CopyableTreeMap<K,V>());
    }

    public CopyOnWriteMQWFTreeMap(Comparator<? super K> comparator) {
        super(new CopyableTreeMap<K,V>(comparator));
    }

    public CopyOnWriteMQWFTreeMap(Map<? extends K, ? extends V> m) {
        super(new CopyableTreeMap<K,V>(m));
    }
    
    public CopyOnWriteMQWFTreeMap(SortedMap<K, ? extends V> m) {
        super(new CopyableTreeMap<K,V>(m));
    }

    
    
    /*
     * Read-only methods
     */
    
    public int size() { 
        return combinedRef.instance.size(); 
    }
    
    public boolean containsKey(Object key) { 
        return combinedRef.instance.containsKey(key); 
    }

    public boolean containsValue(Object value) {
        return combinedRef.instance.containsValue(value);
    }
    
    public V get(Object key) {
        return combinedRef.instance.get(key);
    }
    
    public Comparator<? super K> comparator() {
        return combinedRef.instance.comparator();
    }
    
    public K firstKey() {
        return combinedRef.instance.firstKey();
    }

    public K lastKey() {
        return combinedRef.instance.lastKey();
    }    
    
    //final Entry<K,V> getEntry(Object key) { 
    //final Entry<K,V> getEntryUsingComparator(Object key) {
    //final Entry<K,V> getCeilingEntry(K key) {
    //final Entry<K,V> getFloorEntry(K key) {
    //final Entry<K,V> getHigherEntry(K key) {
    //final Entry<K,V> getLowerEntry(K key) {
    
    public Object clone() {
        return combinedRef.instance.clone();        
    }

    public Map.Entry<K,V> firstEntry() {
        return combinedRef.instance.firstEntry();
    }
    
    public Map.Entry<K,V> lastEntry() {
        return combinedRef.instance.lastEntry();
    }

    public Map.Entry<K,V> lowerEntry(K key) {
        return combinedRef.instance.lowerEntry(key);
    }
    
    public K lowerKey(K key) {
        return combinedRef.instance.lowerKey(key);
    }
    
    public Map.Entry<K,V> floorEntry(K key) {
        return combinedRef.instance.floorEntry(key);
    }
    
    public K floorKey(K key) {
        return combinedRef.instance.floorKey(key);
    }
    
    public Map.Entry<K,V> ceilingEntry(K key) {
        return combinedRef.instance.ceilingEntry(key);
    }
    
    public K ceilingKey(K key) {
        return combinedRef.instance.ceilingKey(key);
    }

    public Map.Entry<K,V> higherEntry(K key) {
        return combinedRef.instance.higherEntry(key);
    }
    
    public Set<K> keySet() {
        return combinedRef.instance.keySet();
    }
    
    public NavigableSet<K> navigableKeySet() {
        return combinedRef.instance.navigableKeySet();
    }
    
    public NavigableSet<K> descendingKeySet() {
        return combinedRef.instance.descendingKeySet();
    }
    
    public Collection<V> values() {
        return combinedRef.instance.values();
    }
    
    public Set<Map.Entry<K,V>> entrySet() {
        return combinedRef.instance.entrySet();
    }
    
    public NavigableMap<K, V> descendingMap() {
        return combinedRef.instance.descendingMap();
    }
    
    public NavigableMap<K,V> subMap(K fromKey, boolean fromInclusive,
                                    K toKey,   boolean toInclusive) {
        return combinedRef.instance.subMap(fromKey, fromInclusive, toKey, toInclusive);
    }
    
    public NavigableMap<K,V> headMap(K toKey, boolean inclusive) {
        return combinedRef.instance.headMap(toKey, inclusive);
    }
    
    public NavigableMap<K,V> tailMap(K fromKey, boolean inclusive) {
        return combinedRef.instance.tailMap(fromKey, inclusive);
    }
    
    public SortedMap<K,V> subMap(K fromKey, K toKey) {
        return combinedRef.instance.subMap(fromKey, toKey);
    }
    
    public SortedMap<K,V> headMap(K toKey) {
        return combinedRef.instance.headMap(toKey);
    }
    
    public SortedMap<K,V> tailMap(K fromKey) {
        return combinedRef.instance.tailMap(fromKey);
    }
        
    public boolean isEmpty() {
        return (combinedRef.instance.size() == 0);    
    }
    
        
    
    
    /*
     * Mutative methods
     */
    
    public void putAll(Map<? extends K, ? extends V> map) {
        applyMutation(null, null, null, map, null, (K _key, V _value, V _v2, Object _map, Object _o2, TreeMap<K,V> tree) -> {
        tree.putAll((Map<? extends K, ? extends V>)_map); 
        return 0;
        });
    }
    
    public V put(K key, V value) {
        return applyMutation(key, value, null, null, null, (K _key, V _value, V _v2, Object _o1, Object _o2, TreeMap<K,V> tree) -> 
        tree.put(_key, _value)); 
    }
    
    public V remove(Object key) {
        return applyMutation((K)key, null, null, null, null, (K _key, V _value, V _v2, Object _o1, Object _o2, TreeMap<K,V> tree) -> 
        tree.remove(_key)); 
    }
    
    public void clear() {
        applyMutation(null, null, null, null, null, (K _key, V _value, V _v2, Object _o1, Object _o2, TreeMap<K,V> tree) -> { 
        tree.clear(); 
        return 0;
        });         
    }

    public Map.Entry<K,V> pollFirstEntry() {
        return applyMutation(null, null, null, null, null, (K _key, V _value, V _v2, Object _o1, Object _o2, TreeMap<K,V> tree) -> 
        tree.pollFirstEntry()); 
    }
    
    public Map.Entry<K,V> pollLastEntry() {
        return applyMutation(null, null, null, null, null, (K _key, V _value, V _v2, Object _o1, Object _o2, TreeMap<K,V> tree) -> 
        tree.pollLastEntry()); 
    }
    
    public boolean replace(K key, V oldValue, V newValue) {
        return applyMutation(key, oldValue, newValue, null, null, (K _key, V _v1, V _v2, Object _o1, Object _o2, TreeMap<K,V> tree) -> 
        tree.replace(_key, _v1, _v2));         
    }
    
    public V replace(K key, V value) {
        return applyMutation(key, value, null, null, null, (K _key, V _v1, V _v2, Object _o1, Object _o2, TreeMap<K,V> tree) -> 
        tree.replace(_key, _v1));        
    }
    
    public void forEach(BiConsumer<? super K, ? super V> action) {
        applyMutation(null, null, null, action, null, (K _key, V _v1, V _v2, Object _action, Object _o2, TreeMap<K,V> tree) -> { 
        tree.forEach((BiConsumer<? super K, ? super V>)_action);
        return 0;
        });                
    }
    
    public void replaceAll(BiFunction<? super K, ? super V, ? extends V> function) {
        applyMutation(null, null, null, function, null, (K _key, V _v1, V _v2, Object _function, Object _o2, TreeMap<K,V> tree) -> { 
        tree.replaceAll((BiFunction<? super K, ? super V, ? extends V>)_function);
        return 0;
        });                        
    }


    public V putIfAbsent(K key, V value) {
        return applyMutation(key, value, null, null, null, (K _key, V _value, V _v2, Object _o1, Object _o2, TreeMap<K,V> tree) -> {
            if (tree.containsKey(_key)) return null;
            return tree.put(_key, _value);
        });                        
    }

    /*
    @Override
    public boolean remove(Object key, Object value) {
        Boolean b = applyMutation(key, value, null, null, null, (K _key, V _value, V _v2, Object _o1, Object _o2, TreeMap<K,V> tree) -> {
        if (_key == null)
            throw new NullPointerException();
        //return _value != null && (tree.put(_key, _value) != null);
        return Boolean.TRUE;        
        });                        
    }
    */
    
    // View and iterator methods to be implemented
    
}
