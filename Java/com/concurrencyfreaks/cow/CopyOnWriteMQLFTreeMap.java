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

import java.util.TreeMap;

/**
 * <h1> TreeMap with a Copy-On-Write with Mutation Queue (Lock-Free) </h1>
 * 
 * TODO:
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 *
 */
public class CopyOnWriteMQLFTreeMap<K,V> extends COWMutationQLF<K, V, K, CopyOnWriteMQLFTreeMap.CopyableTreeMap<K,V>>{
    
    public static class CopyableTreeMap<K,V> extends TreeMap<K,V> implements IShallowCopy {
        private static final long serialVersionUID = -3775955588768508232L;
        public CopyableTreeMap() { super(); }
        public CopyableTreeMap(CopyableTreeMap<K,V> tree) { super(tree); }
        @Override
        public Object copyOf() {
            return new CopyableTreeMap<K,V>(this);
        }
    }
    
    public CopyOnWriteMQLFTreeMap() {
        super(new CopyableTreeMap<K,V>());
    }
    
    /**
     * There is a volatile load in ref.get()
     * 
     * Progress Condition: Wait-Free Population Oblivious
     * 
     * @param key
     * @return
     */
    public V get(Object key) {
        return combinedRef.instance.get(key);
    }
    
    public boolean containsKey(Object key) {
        return combinedRef.instance.containsKey(key);
    }
    
    public V put(K key, V value) {
        return applyMutation(key, value, null, (K _key, V _value, K _nop, TreeMap<K,V> tree) -> tree.put(_key, _value));
    }
    
    
    public V remove(K key) {
        return applyMutation(key, null, null, (K _key, V _value, K _nop, TreeMap<K,V> tree) -> tree.remove(_key));
    }
  
}
