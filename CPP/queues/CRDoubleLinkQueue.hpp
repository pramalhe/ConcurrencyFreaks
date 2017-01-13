/******************************************************************************
 * Copyright (c) 2014-2017, Pedro Ramalhete, Andreia Correia
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

#ifndef _CR_DOUBLE_LINK_QUEUE_HPDL_H_
#define _CR_DOUBLE_LINK_QUEUE_HPDL_H_

#include <atomic>
#include <stdexcept>
#include "HazardPointersDL.hpp" // We need to use a special version of hazard pointers


/**
 * <h1> Double Linked Queue with special Hazard Pointers </h1>
 *
 * Double Linked Queue based on the short paper by Andreia Correia and Pedro Ramalhete
 * https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/doublelink-2016.pdf
 *
 * <p>
 * When running uncontended, this algorithm does one CAS and one store to enqueue
 * and one CAS to dequeue, thus improving over Michael-Scott which does two CAS
 * to enqueue.
 * The dequeue mechanism is pretty much the same as the one on Michael-Scott,
 * with one CAS to dequeue.
 * <p>
 * enqueue algorithm: Double-linked enqueue with relaxed store
 * dequeue algorithm: Michael-Scott
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Pointers with scan for next and prev
 * Uncontended enqueue: 1 CAS + 1 HP
 * Uncontended dequeue: 1 CAS + 1 HP
 * <p>
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class CRDoubleLinkQueue {

private:
    struct Node {
        T* item;
        Node* prev;
        std::atomic<Node*> next;
        Node(T* item) : item{item}, prev{nullptr}, next{nullptr} { }
    };

    bool casTail(Node *cmp, Node *val) {
		return tail.compare_exchange_strong(cmp, val);
	}

    bool casHead(Node *cmp, Node *val) {
        return head.compare_exchange_strong(cmp, val);
    }

    // Pointers to head and tail of the list
    alignas(128) std::atomic<Node*> head;
    alignas(128) std::atomic<Node*> tail;

    static const int MAX_THREADS = 128;
    const int maxThreads;
    // We need one hazard pointer for this algorithm (1 for enqueue and 1 for dequeue)
    HazardPointersDL<Node> hp {maxThreads};


public:
    CRDoubleLinkQueue(int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        Node* sentinelNode = new Node(nullptr);
        // The sentinel node starts self-linked in prev
        sentinelNode->prev = sentinelNode;
        head.store(sentinelNode);
        tail.store(sentinelNode);
    }


    ~CRDoubleLinkQueue() {
        while (dequeue(0) != nullptr); // Drain the queue
        delete head.load();            // Delete the last node
    }

    std::string className() { return "CRDoubleLinkQueue"; }

    void enqueue(T* item, const int tid) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        Node* newNode = new Node(item);
        while (true) {
            Node* ltail = hp.protectPtr(tail.load(), tid);
            if (ltail != tail.load()) continue;
            Node* lprev = ltail->prev; // lprev is protected by the hp
            newNode->prev = ltail;
            // Try to help the previous enqueue to complete
            if (lprev->next.load() == nullptr) lprev->next.store(ltail, std::memory_order_relaxed);
            if (casTail(ltail, newNode)) {
                ltail->next.store(newNode, std::memory_order_release);
                hp.clear(tid);                           // There is a release store here
                return;
            }
        }
    }


    T* dequeue(const int tid) {
        while (true) {
            Node* lhead = hp.protectPtr(head.load(), tid);
            if (lhead != head.load()) continue;
            Node* lnext = lhead->next.load(); // lnext is protected by the hp
            if (lnext == nullptr) {           // Check if queue is empty
                hp.clear(tid);
                return nullptr;
            }
            if (casHead(lhead, lnext)) {
                // lnext may be deleted after we do hp.clear()
                T* item = lnext->item;
                hp.clear(tid);
                hp.retire(lhead, tail.load(), tid);
                return item;
            }
        }
    }
};

#endif /* _CR_DOUBLE_LINK_QUEUE_HPDL_H_ */
