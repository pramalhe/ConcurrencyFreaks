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

import java.lang.reflect.Field;


/**
 * <h1>HarrisRTTILinkedList</h1>
 * Harris's Linked List with Run-Time-Type-Information
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Lock-Free
 * <li>remove(x)   - Lock-Free
 * <li>contains(x) - Lock-Free
 * </ul><p>
 * Lock-Free Linked List as described in 
 * http://research.microsoft.com/pubs/67089/2001-disc.pdf
 * <p>
 * Using RTTI is one of the alternatives to implement Harris's linked list
 * on managed systems. The existence of this technique is described in the
 * following paper, but there is no available source code that we're aware of:
 * <a href="http://cs.brown.edu/~mph/HellerHLMSS05/2005-OPODIS-Lazy.pdf">
 * A Lazy Concurrent List-Based Set Algorithm</a>
 * <p>
 * More info on this post:
 * ???
 * 
 * TODO: Some optimizations can still be done to improve performance.
 * 
 * @author Andreia Correia
 * @author Pedro Ramalhete
 */
public class HarrisRTTILinkedList<E> {

    final Node<E> head;
    final Node<E> tail;
    final Node<E> tailMarked;
    
    abstract static class Node<E>{
        
        public E key;
        public volatile Node<E> next;
                
        /**
         * @param cmp Previous {@code next}
         * @param val New {@code next}
         * @return {@code true} if CAS was successful
         */
        boolean casNext(Node<E> cmp, Node<E> val) {
            return UNSAFE.compareAndSwapObject(this, nextOffset, cmp, val);
        }
        
        // Unsafe mechanics
        private static final sun.misc.Unsafe UNSAFE;
        private static final long nextOffset;

        static {
            try {
                Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
                f.setAccessible(true);
                UNSAFE = (sun.misc.Unsafe) f.get(null);
                Class<?> k = Node.class;
                nextOffset = UNSAFE.objectFieldOffset
                        (k.getDeclaredField("next"));
            } catch (Exception e) {
                System.out.println("In Eclipse, add -Xbootclasspath/a:./bin/ to fix this exception\n");                
                throw new Error(e);
            }
        }
    }
    
    static class NodeUnmarked<E> extends Node<E>{
        NodeMarked<E> nodeMk;
        NodeUnmarked(E key) {
            this.key = key;
            this.next = null;
            nodeMk = new NodeMarked<E>(key, next, this);
        }
    }
    
    static class NodeMarked<E> extends Node<E>{
        final NodeUnmarked<E> nodeUn;
        NodeMarked(E key, Node<E> next, NodeUnmarked<E> nodeUn) {
            this.key = key;
            this.next = null;
            this.nodeUn = nodeUn;
        }
    }
    
    
    
    public HarrisRTTILinkedList() {
        head = new NodeUnmarked<E>(null);
        tail = new NodeUnmarked<E>(null);
        tailMarked = new NodeMarked<E>(null,null,(NodeUnmarked<E>)tail);
        ((NodeUnmarked<E>)tail).nodeMk = (NodeMarked<E>)tailMarked;
        head.next = tail;
    }
   
    
    /**
     * 
     * @param key
     * @return
     */
    public boolean insert(E key) {
        final NodeUnmarked<E> newNode = new NodeUnmarked<E>(key);
        Node<E> leftNode[] = new Node[1];

        while (true) {
            final Node<E> rightNode = search(key, leftNode);
            
            if (rightNode != tail && rightNode.key == key) // T1
                return false;
            newNode.next = rightNode;
            newNode.nodeMk.next = rightNode;
            if (leftNode[0].casNext(rightNode, newNode)) // C2
            {
                return true;
            }
        } // B3        
    }
    
    
    /**
     * 
     * @param key
     * @return
     */
    public boolean delete(E key) {
        Node<E> rightNode; 
        Node<E> rightNodeNext;
        Node<E> leftNode[] = new Node[1];
        while (true) {
            rightNode = search(key, leftNode);
            if ((rightNode == tail) || (rightNode.key != key)) { // T1
                return false;
            }
            rightNodeNext = rightNode.next;
            
            if (rightNodeNext instanceof NodeUnmarked){
                if (rightNode.casNext(rightNodeNext, ((NodeUnmarked<E>)rightNodeNext).nodeMk)){
                    break;
                }
            }
        } // B4
        
        if (!leftNode[0].casNext(rightNode, rightNodeNext)) // C4
        {
            rightNode = search(rightNode.key, leftNode);
        }
        return true;
    }
    

    /**
     * 
     * @param key
     * @return
     */
    public boolean find(E key) {
        Node<E> rightNode;
        Node<E> leftNode[] = new Node[1];
        rightNode = search(key, leftNode);
        return (rightNode != tail && rightNode.key == key);        
    }

    
    private Node<E> search(E key, Node<E> leftNode[]) {
        Node<E> rightNode;
        Node<E> leftNodeNext = null;
        
        searchAgain:
        while (true) {
            Node<E> t = head;
            Node<E> tnext = head.next;
            
            @SuppressWarnings("unchecked")
            Comparable<? super E> keyComp = (Comparable<? super E>) key;         
            // 1: find leftNode and rightNode
            do {
                if (tnext instanceof NodeUnmarked) {
                    leftNode[0] = t;
                    leftNodeNext = tnext;
                    t = tnext;
                } else {
                    t = ((NodeMarked<E>)tnext).nodeUn;
                }
                if (t == tail) {
                    break;
                }
                tnext = t.next;
            } while ((tnext instanceof NodeMarked) || keyComp.compareTo(t.key) > 0);
            rightNode = t;
            
            // 2: Check nodes are adjacent
            if (leftNodeNext == rightNode) {
                if (rightNode != tail && rightNode.next instanceof NodeMarked) {
                    continue searchAgain; // G1
                } else { 
                    return rightNode; // R1
                }
            }
            
            // 3: Remove one or more marked nodes
            if (leftNode[0].casNext(leftNodeNext, rightNode)) // C1
                if (rightNode != tail && rightNode.next instanceof NodeMarked) {
                    continue searchAgain; // G2
                } else { 
                    return rightNode; // R2
                }
        } // B2
    }
    
    public boolean add(E key) { return insert(key); }
    public boolean remove(E key) { return delete(key); }
    public boolean contains(E key) { return find(key); }
}
