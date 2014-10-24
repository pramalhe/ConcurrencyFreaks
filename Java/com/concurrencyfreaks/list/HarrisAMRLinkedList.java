/******************************************************************************
 * Copyright (c) 2014, Pedro Ramalhete, Andreia Correia
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
package com.concurrencyfreaks.list;

import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * <h1>HarrisAMRLinkedList</h1>
 * Harris's Linked List with AtomicMarkableReference.
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Lock-Free
 * <li>remove(x)   - Lock-Free
 * <li>contains(x) - Wait-Free
 * </ul><p>
 * Lock-Free Linked List as described in Harris's paper:
 * {@link http://research.microsoft.com/pubs/67089/2001-disc.pdf}
 * <a href="http://research.microsoft.com/pubs/67089/2001-disc.pdf">Harris's paper</a>
 * <p>
 * This is based on the implementation that uses AtomicMarkableReference and 
 * with contains() that does not help add/remove, as described on the book
 * "The Art of Multiprocessor Programming". See figures 9.24, 9.25, 9.26, 9.27 
 *
 * <p>
 * Memory usage per key: One "Node" object with two pointers, one 
 * AtomicMarkableReference instance with one reference, one Pair instance with 
 * one reference and one boolean. 
 * 32 bit machine: 4 x (2+2 + 1+2 + 2+2) = 44 bytes per key
 * 64 bit machine: 8 x (2+2 + 1+2 + 2+2) = 88 bytes per key
 * Notice that Objects in Java use at least 2 words.
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
public class HarrisAMRLinkedList<E> {

    final Node<E> head;
    final Node<E> tail;
    
    static class Node<E> {
        final E key;
        final AtomicMarkableReference<Node<E>> next;
               
        Node(E key) {
            this.key = key;
            this.next = new AtomicMarkableReference<Node<E>>(null, false);
        }
    }
    
    // Figure 9.24, page 216
    static class Window<T> {
        public Node<T> pred;
        public Node<T> curr;
        
        Window(Node<T> myPred, Node<T> myCurr) {
            pred = myPred; 
            curr = myCurr;
        }
    }
    
    
    public HarrisAMRLinkedList() {
        tail = new Node<E>(null);
        head = new Node<E>(null);
        head.next.set(tail, false);
    }
    
    
    /**
     * Taken from Figure 9.25, page 217
     * For more info take a look at Fig 2 from Harris's paper
     * <p>
     * Progress Condition: Lock-Free
     * 
     * @param key
     * @return
     */
    public boolean add(E key) {
        final Node<E> newNode = new Node<E>(key);
        while (true) {
            final Window<E> window = find(key);
            // On Harris paper, pred is named left_node and curr is right_node
            final Node<E> pred = window.pred;
            final Node<E> curr = window.curr;
            if (curr.key == key) { 
                return false;
            } else {
                newNode.next.set(curr, false);
                if (pred.next.compareAndSet(curr, newNode, false, false)) {
                    return true;
                }
            }
        }       
    }

    
    /**
     * Inspired by Figure 9.26, page 218 on "The Art of Multiprocessor Programming".
     * For more info take a look at Fig 3 from Harris's paper.
     * <p>
     * Progress Condition: Lock-Free
     * 
     * @param key
     * @return
     */
    public boolean remove(E key) {
        while (true) {
            final Window<E> window = find(key);
            // On Harris's paper, "pred" is named "left_node" and the "curr"
            // variable is named "right_node".            
            final Node<E> pred = window.pred;
            final Node<E> curr = window.curr;
            if (curr.key != key) {
                return false;
            } 
            final Node<E> succ = curr.next.getReference();
            // In "The Art of Multiprocessor Programming - 1st edition", 
            // the code shown has attemptMark() but we can't use it, 
            // because attemptMark() returns true if the node
            // is already marked, which is not the desired effect, so we 
            // must use compareAndSet() instead.
            if (!curr.next.compareAndSet(succ, succ, false, true)) {
                continue;
            }
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    
    /**
     * Inspired by Figure 9.24, page 216 
     * <p>
     * Progress Condition: Lock-Free
     *      
     * @param key
     * @return
     */
    public Window<E> find(E key) {
        Node<E> pred = null;
        Node<E> curr = null; 
        Node<E> succ = null;
        boolean[] marked = {false};
        @SuppressWarnings("unchecked")        
        final Comparable<? super E> keyComp = (Comparable<? super E>)key; 
        
        // I think there is a special case for an empty list
        if (head.next.getReference() == tail) {
            return new Window<E>(head, tail);
        }
        
        retry: 
        while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        continue retry;
                    }
                    curr = succ;
                    succ = curr.next.get(marked);
                }

                if (curr == tail || keyComp.compareTo(curr.key) <= 0) {
                    return new Window<E>(pred, curr);
                }
                pred = curr;
                curr = succ;
            }
        }
    }


    
    /**
     * Searches for a given key.
     * 
     * Inspired by Figure 9.27, page 218 on "The Art of Multiprocessor Programming".
     * 
     * As soon as we find a matching key we immediately return false/true 
     * depending whether the corresponding node is marked or not. We can do 
     * this because add() will always insert new elements immediately after a
     * non-marked node.
     * <p>
     * Progress Condition: Wait-Free - bounded by the number of nodes 
     * 
     * @param key
     * @return
     */
    public boolean contains(E key) {
        boolean[] marked = {false};
        Node<E> curr = head.next.getReference();
        curr.next.get(marked);
        @SuppressWarnings("unchecked")
        final Comparable<? super E> keyComp = (Comparable<? super E>)key; 
        while (curr != tail && keyComp.compareTo(curr.key) > 0) {
            curr = curr.next.getReference();
            curr.next.get(marked);
        }
        return (curr.key == key && !marked[0]); 
    }
}
