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

import core.atomic;
import core.memory;
import std.stdio;



/**
 * <h1>Concurrent Linked List with Elected Unlink</h1>
 * A Linked List where add() are always done at the end of the list
 * and the remove() can be done anywhere but the unlinking operation
 * is done with an Elected pattern.
 * <p>
 * This implementation supports three major operations, whose Progress 
 * Conditions are:
 * <ul>
 * <li>{@code add()} - Lock-Free
 * <li>{@code remove()} - Lock-Free
 * <li>{@code contains()} - Lock-Free
 * </ul>
 * <p>
 * <ul>
 * There are at least two non-trivial details in this data structure:
 * <li> <strong>Election Mechanism</strong> -
 *      This is a "Special One Pattern", also called "Election Pattern", and
 *      it consists of using an atomic variable, <code>unlinkGuard</code>, to 
 *      <i>elect</i> a thread that will be given a <i>special</i> task. 
 *      In this case, the special task is to unlink nodes whose 
 *      <code>state</code> is {@code REMOVED}.
 * 
 * <li> <strong>List Traversal with Relaxed Atomics</strong> -
 *      The member Node.next is not qualified as a <code>shared</code>, instead, 
 *      this is a <b>relaxed atomic</b>. We need to do an acquire-barrier if 
 *      Node.next is null, because Node.next can only take two values: null 
 *      (initial value), or a reference to the next Node instance in the list. 
 *      Once it has been assigned a non-null reference, the Node.next can only 
 *      be assigned another non-null reference.            
 * </ul>
 *
 * Compared to other lock-free linked lists like Java's ConcurrentLinkedQueue, 
 * this data structure is not as "GC friendly" because each node that is 
 * unlinked, has a {@code next} that references another node that is possibly 
 * still in the linked list. <br>
 * One way to address this is to use the same trick that the CLQ uses, and set
 * the <code>next</code> to itself after a node is unlinked. This would force 
 * traversal operations to restart from the head if they find a <code>next</code> 
 * that points to the same node. See CLQ.contains() and CLQ.succ() for an 
 * example of how this works.
 * <p>
 * The main advantage of this algorithm when compared with previously known
 * lock-free lists, is that the traversal of the list can be done without 
 * any barriers, at least until a <code>next</code> with null or a matching 
 * <code>item</code> are found. <br>
 * This means that for algorithms like the one on the CLQ, the traversal of
 * the list implies O(n) acquire barriers, while for the CLLElectedUnlink 
 * algorithm it is O(1) acquire barriers. <br>
 * On architectures like x86, where a an acquire barrier in comes for free, 
 * this won't make any difference in terms of performance, but in architectures
 * with a more relaxed model where the acquire barriers have a price (like ARM
 * or PowerPC), this may be able to provide some gains. <br>
 * How big are those gains depends a lot on the difference in <i>cost</i>
 * between using a load with acquire barrier or a relaxed atomic load on that 
 * particular architecture. 
 * <p>
 * More info at:
 * http://concurrencyfreaks.com/2014/06/cllelectedunlink-lock-free-list-with.html
 * <p>
 * TODO: Add some "const" or "immutable"
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
class CLLElectedUnlinkGC(T) {

    // Possible states of Node.state
    private static const int INUSE   = 0;
    private static const int REMOVED = 1;
    
    // Possible states of unlinkGuard
    private static const long NO_GUARD = 0;
    private static const long GUARDED  = 1;
    
    // Possible states of unlinkNeeded
    private static const long NO_NEED     = 0;
    private static const long NEED_UNLINK = 1;
        
    // This is the "lock" of the Elected Pattern that protects the unlinking
    // operation. Its states can be NO_GUARD or GUARDED.
    // For more details, see this this post on the Elected Pattern:
    // http://concurrencyfreaks.com/2014/06/elected-pattern.html       
    private shared(long) unlinkGuard;
    
    // This is a flag to tell the next remove() if an unlink is needed or not.
    // States can be NO_NEED or NEED_UNLINK.
    private shared(long) unlinkNeeded;
    
    // Reference to the Node at the start of the list. Traversals of the linked 
    // list start from this node. 
    private shared(Node*) head;
    
    // Reference to the Node at the end of the list. Used by add() to insert new
    // Nodes into the linked list.
    private shared(Node*) tail;

    /**
     * Inner class for Nodes
     * <p>
     * Some notes on the member variables:
     * <ul> 
     * <li> "item" is immutable because once assigned, we don't want to 
     * change it. This will save us some acquire barriers.
     * <li> "next" is <b>not</b> shared because we only want to do the 
     * acquire and release barriers in certain places. We don't want to force 
     * an acquire barrier when traversing the list, until we reach the 
     * original tail.
     * <li> "state" is shared and will be modified only with a CAS, so 
     * that only one thread can set it to <b>REMOVED</b>. Possible states of 
     * this variable are <b>INUSE</b> or <b>REMOVED</b>.
     * </ul>
     */
    static struct Node {
    	T key;
    	shared(Node*) next;
    	shared(int) state;
    }


    this() {
    	unlinkGuard  = NO_GUARD;
    	unlinkNeeded = NO_NEED;
        // Start with the head and tail having a sentinel node
        head = tail = cast(shared)(new Node);
    }
    
    /**
     * Set Node's state to <b>REMOVED</b>
     * 
     * @return true if CAS was successful and this thread was the 
     * responsible for doing the (logical) removal of the element from
     * the linked list.
     */
    private bool markRemoved(Node* node) {
    	return cas(&node.state, INUSE, REMOVED);
    }

    /**
     * Checks if the Node has been marked as REMOVED already.
     * @return true if the Node is already marked as REMOVED
     */
    private bool isRemoved(Node node) {
        return (node.state == REMOVED);
    }
    
    /**
     * This will modify Node.next, which is not volatile, but 
     * compareAndSwapObject() will take care of doing the release-barrier.
     * 
     * @param cmp Previous next
     * @param val New next
     * @return {@code true} if CAS was successful
     */
    private bool casNext(Node *node, Node* oldnext, Node* newnext) {
        return cas(&node.next, cast(shared)oldnext, cast(shared)newnext);
    }

    private bool casUnlinkGuard(int oldval, int newval) {
        return cas(&unlinkGuard, oldval, newval);
    }

    private bool casUnlinkNeeded(int oldval, int newval) {
        return cas(&unlinkNeeded, oldval, newval);
    }
    
    private bool casTail(Node* oldtail, Node* newtail) {
        return cas(&tail, cast(shared)oldtail, cast(shared)newtail);
    }

    private bool casHead(Node* oldhead, Node* newhead) {
        return cas(&head, cast(shared)oldhead, cast(shared)newhead);
    }    


    /**
     * Adds "key" to the end of the list. 
     * 
     * Progress Condition: Lock-Free
     * 
     * @param key
     */
    public bool add(T key) {
    	if (key == null) return false;
        Node* newNode = new Node();
        newNode.key = key;
        Node* nullNode = null;
        while (true) {
            Node* localTail = cast(Node*)tail;
            Node* node = cast(Node*)localTail.next;
            if (localTail == cast(Node*)tail) {
                if (node == null) {
                    // It seems this is the last node, so add the newNode here 
                    // and try to move the tail to the newNode
                    if (casNext(localTail, nullNode, newNode)) {
                        casTail(localTail, newNode); // Failure is OK.
                        return true;
                    }
                } else {
                    casTail(localTail, node);
                }
            }
        }
    }

    /**
     * Search for "item" in the list.
     * <p>
     * Progress Condition: Lock-Free
     * 
     * @param item
     * @return <b>true</b> if "item" is in the list, and <b>false</b> 
     * if it is not there, or is there but marked as <b>REMOVED</b>
     */
    public bool contains(T key) {
        if (key == null) return false;
        Node* node = cast(Node*)head;
        while (node != null) {
            if (key == node.key && node.state == INUSE) {
                return true;
            }
            // No need for acquire-barriers unless we see null
            Node* nnext = atomicLoad!(MemoryOrder.raw)(node.next);
            node = (nnext == null) ? cast(Node*)node.next : nnext;
        }
        return false;
    }


    /**
     * Logically removes elements from the list.
     * <p>
     * When searching the list, uses the same optimization of relaxed atomics 
     * as contains().
     * <p>
     * Progress Condition: Lock-Free
     * 
     * @param item
     * @return {@code true} if {@code item} was found and was not already in 
     * {@code REMOVED} state and was successfully set to {@code REMOVED}
     */
    public bool remove(T key) {
        if (key == null) return false;
        if (unlinkGuard == NO_GUARD && casUnlinkGuard(NO_GUARD, GUARDED)) {
            try {
                // We got the hold on the guard, now figure out if unlinking is
                // needed for other nodes or just this one.
                if (unlinkNeeded == NEED_UNLINK) {
                    return markOneAndUnlinkAll(key);               
                } else {
                    return markAndUnlinkOne(key);
                }
            } finally {
                unlinkGuard = NO_GUARD;
            }
        } else {
            // Didn't get the hold of the guard, so mark the node and don't
            // do any unlinking.
            return markNode(key);
        }
    }

    /**
     * Check each node on the list and unlink all whose's state is {@code REMOVED}.
     * <p>
     * This should be called only from {@code remove()} and under the 
     * protection of {@code unlinkGuard}.
     * <p>
     * Progress Condition: Lock-Free 
     * 
     * @param item
     * @return
     */
    private bool markOneAndUnlinkAll(T key) {
        casUnlinkNeeded(NEED_UNLINK, NO_GUARD); 
        Node* node = cast(Node*)head;
        Node* lastInUse = cast(Node*)head;
        Node* localTail = cast(Node*)tail;
        bool doUnlinking = true; // Turns to false once the localTail has been reached
        bool nodeWasMarked = false;
        
        // Lets loop first to advance the head if needed
        while (head.state == REMOVED) {
            Node* headNext = cast(Node*)head.next; 
            if (headNext == null) return false;
            head = cast(shared)headNext;
        }
        node = cast(Node*)head;
        
        while (node != null) {
            // We do the order here differently and read the value of "state"
            // before looking at the key.
            if (!nodeWasMarked && node.state == INUSE && key == node.key && markRemoved(node)) {
                // We've marked the node, but instead of returning true 
                // immediately, we continue to search the linked list for 
                // nodes that need to be unlinked.
                nodeWasMarked = true;
                // Unlink only if it is not the last node nor the first
                if (lastInUse != cast(Node*)head && node.next != null) {
                    lastInUse.next = atomicLoad!(MemoryOrder.raw)(node.next);
                }
            }            
            // Once the tail has been found, don't do any more unlinking 
            if (node == localTail) doUnlinking = false;
            // Optimization: If the tail has been reached and the item has been 
            // found, then we can return true immediately.
            if (!doUnlinking && nodeWasMarked) return true;
            if (doUnlinking) {
                if (node.state == INUSE) {
                    lastInUse = node;
                } else {
                	Node* nodenext = atomicLoad!(MemoryOrder.raw)(node.next);
                	atomicStore!(MemoryOrder.raw)(lastInUse.next, cast(shared)nodenext);
                }
            }
            Node* nnext = atomicLoad!(MemoryOrder.raw)(node.next);
            node = (nnext == null) ? cast(Node*)node.next : nnext;            
        }            
        return nodeWasMarked;
    }

   /**
     * Find the node with "item", mark it and unlink it.
     * <p>
     * When there is no other unlink to do, then go over the linked list 
     * searching for the node with "item", mark that node and unlink it.
     * Don't even try to look for unlinked nodes because that will make
     * the operation slower.
     * <p>
     * This should be called only from {@code remove()} and under the 
     * protection of {@code unlinkGuard}.
     * <p>
     * Progress Condition: Lock-Free 
     * 
     * @param item
     * @return
     */
    private bool markAndUnlinkOne(T key) {
        Node* node = cast(Node*)head;
        Node* prev = cast(Node*)head;
        while (node != null) {
            if (key == node.key && node.state == INUSE) {
                if (markRemoved(node)) {
                	Node* nodenext = atomicLoad!(MemoryOrder.raw)(node.next);
                    if (node == cast(Node*)head && nodenext != null) {
                        // Removed node was "head" so try to advance it
                        casHead(node, nodenext);
                    } else if (nodenext != null) {
                        // Unlink only if it is not the last node
                        atomicStore!(MemoryOrder.raw)(prev.next, cast(shared)nodenext);
                    } 
                    return true;
                }
            }
            prev = node;
            Node* nnext = atomicLoad!(MemoryOrder.raw)(node.next);
            node = (nnext == null) ? cast(Node*)node.next : nnext;            
        }            
        return false;
    }
            
   /**
     * Find only the node with "item" and mark it
     * <p>
     * Progress Condition: Lock-Free 
     *         
     * @param item
     * @return
     */
    private bool markNode(T key) {
        Node* node = cast(Node*)head;
        while (node != null) {
            if (key == node.key && node.state == INUSE) {
                if (markRemoved(node)) {
                    unlinkNeeded = NEED_UNLINK;
                    return true;
                }
            }
            Node* nnext = atomicLoad!(MemoryOrder.raw)(node.next);
            node = (nnext == null) ? cast(Node*)node.next : nnext;            
        }
        return false;
    }
    



	unittest {
		auto ceu = new CLLElectedUnlinkGC!(int);
		assert(ceu.add(1));
		assert(ceu.contains(1));
		assert(ceu.add(2));
		assert(ceu.contains(1), "1 is not there");
		assert(ceu.contains(2), "2 is not there");
		assert(ceu.add(3));
		assert(ceu.remove(1));
		assert(!ceu.contains(1), "1 is still there");
		assert(ceu.contains(2),  "2 is not there");
		assert(ceu.contains(3),  "3 is not there");
		writeln("It works!!!\n");
	}

}


