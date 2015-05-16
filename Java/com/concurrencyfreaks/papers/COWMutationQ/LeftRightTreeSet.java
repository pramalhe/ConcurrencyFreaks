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
package com.concurrencyfreaks.papers.COWMutationQ;

import java.util.Collection;
import java.util.TreeSet;

import com.concurrencyfreaks.leftright.experimental.LRClassical;
import com.concurrencyfreaks.readindicators.RIArrayCLQFinalizers;

/**
 * <h1> TreeSet with a Classical Left-Right pattern </h1>
 * 
 * This data structure uses the CLQ+Array+Finalizers ReadIndicators
 * 
 * TODO:
 * 
 * @author Pedro Ramalhete
 * @author Andreia Correia
 *
 */
public class LeftRightTreeSet<E> extends LRClassical<E, Collection<? extends E>, E, TreeSet<E>> implements ITreeSet<E> {
        
    public LeftRightTreeSet() {
        super(new TreeSet<E>(), new TreeSet<E>(), new RIArrayCLQFinalizers(), new RIArrayCLQFinalizers());
    }
    
    /**
     * Progress Condition: Wait-Free Population Oblivious
     */
    public boolean contains(Object key) {
        return applyReadOnly((E)key, null, null, (E _key, Collection<? extends E> _nop1, E _nop2, TreeSet<E> tree) -> tree.contains(_key));
    }
        
    /**
     * Progress Condition: Blocking
     */
    public boolean add(E key) {
        return applyMutation(key, null, null, (E _key, Collection<? extends E> _nop1, E _nop2, TreeSet<E> tree) -> tree.add(_key));
    }
        
    /**
     * Progress Condition: Blocking
     */
    public boolean remove(E key) {
        return applyMutation(key, null, null, (E _key, Collection<? extends E> _nop1, E _nop2, TreeSet<E> tree) -> tree.remove(_key));
    }
    
    /**
     * Progress Condition: Blocking
     */
    public boolean addAll(Collection<? extends E> c) {        
        return applyMutation(null, c, null, (E _nop, Collection<? extends E> _c, E _nop2, TreeSet<E> tree) -> tree.addAll(_c) );
    }
}
