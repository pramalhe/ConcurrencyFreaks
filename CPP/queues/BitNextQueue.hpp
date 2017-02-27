/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
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

#ifndef _BIT_NEXT_HP_H_
#define _BIT_NEXT_HP_H_

#include <atomic>
#include <stdexcept>
#include "HazardPointers.hpp"


/**
 * <h1> Bit Next Queue </h1>
 *
 * A Lock-Free queue which uses a trick similar to Tim Harris lock-free list of
 * setting a bit on the "next" of the node.
 * Based on the paper "BitNext - A Lock-Free Queue"
 * https://github.com/pramalhe/ConcurrencyFreaks/tree/master/papers/bitnext-2016.pdf
 *
 * enqueue algorithm: bit-next, based on the trick of the bit on the next like on Maged-Harris list
 * dequeue algorithm: bit-next, based on the trick of the bit on the next like on Maged-Harris list
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Pointers
 * Uncontended enqueue: 2 CAS + 1 HP
 * Uncontended dequeue: 2 CAS + 1 HP

 * <p>
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 *
 * @author Andreia Correia
 * @author Pedro Ramalhete
 */
template<typename T>
class BitNextQueue {

private:
    struct Node {
        T* item;
        std::atomic<Node*> next;
        Node(T* item) : item{item}, next{nullptr} { }
    };

    bool casTail(Node *cmp, Node *val) {
        return tail.compare_exchange_strong(cmp, val);
    }

    bool casHead(Node *cmp, Node *val) {
        return head.compare_exchange_strong(cmp, val);
    }

    struct HPGuard {
        HazardPointers<Node>& hp;
        const int tid;
        HPGuard(HazardPointers<Node>& hp, const int tid) : hp{hp}, tid{tid} { }
        ~HPGuard() { hp.clear(tid); }
    };

    // Pointers to head and tail of the list
    alignas(128) std::atomic<Node*> head;
    alignas(128) std::atomic<Node*> tail;

    static const int MAX_THREADS = 128;
    const int maxThreads;

    HazardPointers<Node> hp {1, maxThreads}; // We don't traverse the list, so we use just one hp
    const int kHpTail = 0;
    const int kHpHead = 0;

    /*
     * Bit-related functions
     */
    bool isMarked(Node* node) {
        return ((size_t) node & 0x1);
    }

    Node* getMarked(Node* node) {
        return (Node*)((size_t) node | 0x1);
    }

    Node* getUnmarked(Node* node) {
        return (Node*)((size_t) node & (~0x1));
    }


public:
    BitNextQueue(int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        Node* sentinelNode = new Node(nullptr);
        // The sentinel is already "logically removed"
        sentinelNode->next.store(getMarked(nullptr), std::memory_order_relaxed);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
    }


    ~BitNextQueue() {
        while (dequeue(0) != nullptr); // Drain the queue
        delete head.load();            // Delete the last node
    }


    std::string className() { return "BitNextQueue"; }


    /*
     * Progress condition: lock-free
     *
     * If we don't know maxThreads, we can replace the for() loop with a
     * while(true) and it will still be correct.
     */
    void enqueue(T* item, const int tid) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        HPGuard hpguard { hp, tid }; // RAII to call hp.clear(tid) when returning
        Node* newNode = new Node(item);
        while (true) {
            Node* ltail = hp.protectPtr(kHpTail, tail.load(), tid);
            if (ltail != tail.load()) continue;
            Node* lnext = ltail->next.load();
            if (getUnmarked(lnext) != nullptr) {         // Advance the tail first
                casTail(ltail, getUnmarked(lnext));      // "tail" is always unmarked
            } else {
                for (int i=0; i < 2; i++) {
                    Node* newNodeMark = isMarked(lnext) ? getMarked(newNode) : newNode; // lnext here is either nullptr or nullptr|0x1
                    newNode->next.store(nullptr, std::memory_order_relaxed);
                    if (ltail->next.compare_exchange_strong(lnext, newNodeMark)) {
                        casTail(ltail, newNode);
                        return;
                    }
                    lnext = ltail->next.load();
                    if (getUnmarked(lnext) != nullptr) {
                        casTail(ltail, getUnmarked(lnext));      // "tail" is always unmarked
                        break;
                    }
                }
            }
            for (int i = 0; i < maxThreads-1; i++) {       // This loop will run at most maxThreads because the CAS can fail at most maxThreads
                lnext = ltail->next.load();
                if (isMarked(lnext)) break;    // This node has been dequeued, must re-read tail. It's ok to be marked as long as it's the first and therefore, nullptr
                newNode->next.store(lnext, std::memory_order_relaxed);
                if (ltail->next.compare_exchange_strong(lnext, newNode)) return;
            }
        }
    }


    /*
     * Progress condition: lock-free
     *
     * The dequeue() marks the node that has the item as "logically removed"
     * by setting the "marked" bit in node.next
     * By default, the "head" is pointing to the first node that has not been
     * "logically removed", but if it's the last node (node.next is nullptr),
     * then the head will be pointing to the last "logically removed" node.
     */
    T* dequeue(const int tid) {
        HPGuard hpguard { hp, tid }; // RAII to call hp.clear(tid) when returning
        while (true) {
            Node* lhead = hp.protectPtr(kHpHead, head.load(), tid);
            if (lhead != head.load()) continue;
            Node* lnext = lhead->next.load();
            if (isMarked(lnext)) { // This one is marked, help advance the head and go for the next node.
                if (getUnmarked(lnext) == nullptr) return nullptr;  // Don't advance head if this is already the last node
                if (casHead(lhead, getUnmarked(lnext))) hp.retire(lhead, tid);
                continue;
            }
            // By now we are certain lnext is not marked
            if (lhead->next.compare_exchange_strong(lnext, getMarked(lnext))) return lhead->item;
        }
    }
};

#endif /* _BIT_NEXT_HP_H_ */
