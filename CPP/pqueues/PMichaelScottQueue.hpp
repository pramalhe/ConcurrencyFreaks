/******************************************************************************
 * Copyright (c) 2018, Pedro Ramalhete, Andreia Correia
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

#ifndef _PERSISTENT_MICHAEL_SCOTT_QUEUE_HP_H_
#define _PERSISTENT_MICHAEL_SCOTT_QUEUE_HP_H_

#include <atomic>
#include <stdexcept>
#include "pfences.h"
#include "HazardPointers.hpp"


/**
 * <h1> Persistent Michael-Scott Queue </h1>
 *
 * enqueue algorithm: MS enqueue + CR modifications
 * dequeue algorithm: MS dequeue + CR modifications
 * Consistency: Durable Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Pointers (lock-free)
 *
 * This Queue is a modification to persistence of the original lock-free algorithm
 * by Maged Michael and Michael Scott. We reduced the number of PWB() and PFENCE()
 * as much as possible and we've added the logic for the recovery.
 * The enqueue()/dequeue() methods don't need a recovery method, but the constructor
 * and destructor are (by definition) not lock-free and therefore need to handle
 * proper recovery.
 *
 * To understand what the PWB/PFENCE/PSYNC are, take a look at
 * "Preserving Happens-Before in persistent memory":
 * https://www.cs.rochester.edu/u/jhi1/papers/2016-spaa-transform
 *
 * We're assuming that CAS has persistent semantics similar to PFENCE() that
 * doesn't act on the load/store of the CAS itself, only on the other loads
 * and stores. In other words, it's as if a CAS is equivalent to a:
 *   PFENCE();
 *   CAS()     // concurrent
 *   PFENCE();
 * The reason we assume this, is because on x86, LOCK instructions and
 * read-modify-write instructions like CAS, ensure order for CLFLUSHOPT and
 * CLWB (PWBs). For more details see Intel's manual for CLFLUSHOPT:
 * https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-optimization-manual.pdf
 *
 * This algorithm was designed such that on enqueue(), a successful CAS on ltail->next
 * implies that the PWB()s for newNode->item, newNode->next and tail have been done,
 * and a successful CAS on tail means that the PWB() on ltail->next has been done.
 *
 * As for the PWB() and PSYNC() before returning, they're not always needed but
 * it helps to reason about in terms of composability.
 * The only way to observe effects from this queue is to call enqueue() or
 * dequeue(), therefore, the next call to the same method will flush the cache
 * line and persist it.
 * However, if you want to do something like:
 *   q.enqueue(a);
 *   a_is_persisted = true;
 *   PWB(&a_is_persisted);
 * then the only way to guarantee correct ordering, is to have the PWB() and
 * a PSYNC() or PFENCE() before returning from enqueue()/dequeue().
 *
 * Related to the above, there is a trick on enqueue(). Instead of adding the
 * PWB(&tail) and PSYNC(), we don't do that thanks to happens-before relations.
 * Namely, for enqueue() to return it means the CAS on tail has been done, and
 * although the value of tail may not be persisted, the CAS on tail guarantees
 * that the value of the node->next is persisted. This means that if a crash
 * occurs and even if a_is_persisted is persisted and the change to tail occurring
 * on the enqueue does not, it's still ok because the node->next is persisted
 * which will allow the recover() of the queue to advance the tail and persist
 * it as well.
 * Unfortunately, for the dequeue(), no such trick is possible on the head,
 * therefore, we really do need the PWB(&head) and PSYNC() before returning
 * from dequeue().
 *
 * About the constructor:
 * As long as the allocator returns a zeroed-out memory region, the 'head' and
 * 'tail' will be nullptr even if there is a crash immediately at the start of
 * the call to the constructor. If the allocator can't guarantee that the data
 * is zero-ed out, then there is no 100% safe way to distinguish between a
 * proper initialization and trash immediately after* allocating the queue.
 * A crash occurring after the 'head' and 'tail' are made persistent (with nullptr)
 *is always recoverable, although there are a few different cases:
 * - If the head is nullptr then the sentinel node must be allocated;
 * - If the head is non-null but tail is null, then the sentinel was allocated
 *   and assigned to head but not to tail;
 * - If both head and tail are non-null and tail->next is non-null then tail is
 *   not pointing to the last node and we need to advance tail;
 *
 * About the destructor:
 * The destructor must first drain the queue to avoid leaking as much as possible.
 * Then, it needs to de-allocate the last node and zero-out the head to make
 * sure then in the event of a failure, the recovery operation will not try to
 * "recover" the queue, therefore, we have a persistent variable named 'destructorInProgress'
 * which is set before starting the destruction operation.
 * After destructorInProgress has been set to true and ordered with a PFENCE(),
 * we can clear head, and only then can we de-allocate the last node.
 *
 * All allocation and de-allocation operations in this queue are prone to
 * leaking, if the failure occurs immediately before a de-allocation or
 * immediately after an allocation. There is no way around this problem without
 * transactions, and seen as we're trying to get lock-free progress, the
 * transactional mechanism would have to be also lock-free, and there is no
 * lock-free persistent transactional engine published (yet).
 *
 * Even though this is portable C++ code, this is meant for x86 and may not work correctly on other architectures.
 *
 * Maged Michael and Michael Scott's Queue with Hazard Pointers:
 * <p>
 * Lock-Free Linked List as described in Maged Michael and Michael Scott's paper:
 * {@link http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf}
 * <a href="http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf">
 * Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms</a>
 * <p>
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 *
 *
 * TODO: test this on persistence
 *
 */
template<typename T>
class PMichaelScottQueue {

private:
    static const int MAX_THREADS = 128;

    struct Node {
        T* item;
        std::atomic<Node*> next;
        Node(T* userItem) : item{userItem}, next{nullptr} { }
        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    bool casTail(Node *cmp, Node *val) {
		return tail.compare_exchange_strong(cmp, val);
	}

    bool casHead(Node *cmp, Node *val) {
        return head.compare_exchange_strong(cmp, val);
    }

    //
    // Persistent variables
    //

    // Pointers to head and tail of the list
    alignas(128) std::atomic<Node*> head {nullptr};
    alignas(128) std::atomic<Node*> tail {nullptr};
    // Will be set to true when the destructor is called, in case there is a crash during destructor
    bool destructorInProgress = false;
    const int maxThreads;

    // We need two hazard pointers for dequeue()
    // This variable is a non-volatile pointer to a volatile object
    HazardPointers<Node>* hp  = new HazardPointers<Node>{2, maxThreads};
    static const int kHpTail = 0;
    static const int kHpHead = 0;
    static const int kHpNext = 1;


    /*
     * To be called when restarting after a failure
     */
    void recover() {
        if (destructorInProgress) {
            if (head.load(std::memory_order_relaxed) != nullptr) {
                while (dequeue(0) != nullptr); // Drain the queue
                head.store(nullptr, std::memory_order_relaxed);
                PWB(&head);
                PFENCE();
                delete head.load(std::memory_order_relaxed);  // Delete the last node    // TODO: replace this with NVM deallocator
            }
            PSYNC();
            return;
        }
        hp = new HazardPointers<Node>{2, maxThreads};
        // If both head is null then a failure occurred during constructor
        if (head.load(std::memory_order_relaxed) == nullptr) {
            Node* sentinelNode = new Node(nullptr);    // TODO: replace this with NVM allocator
            head.store(sentinelNode, std::memory_order_relaxed);
            PWB(&head);
            PFENCE();
        }
        // If tail is null, then fix it by setting it to head
        if (tail.load(std::memory_order_relaxed) == nullptr) {
            tail.store(head.load(std::memory_order_relaxed), std::memory_order_relaxed);
            PWB(&tail);
            PFENCE();
        }
        // Advance the tail if needed
        Node* ltail = tail.load(std::memory_order_relaxed);
        Node* lnext = ltail->next.load(std::memory_order_relaxed);
        if (lnext != nullptr) {
            tail.store(lnext, std::memory_order_relaxed);
            PWB(&tail);
        }
        PSYNC();
    }


public:
    PMichaelScottQueue(int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        PWB(&head);
        PWB(&tail);
        PFENCE();
        recover();  // re-use the same code as the recovery method
    }

    ~PMichaelScottQueue() {
        destructorInProgress = true;
        PWB(&destructorInProgress);
        PFENCE();
        recover();  // Re-use the same code as in the recovery method
    }

    std::string className() { return "PMichaelScottQueue"; }

    /*
     * Uncontended: at least 3 PWB()s, 2 CAS, and 1 PSYNC()
     */
    void enqueue(T* item, const int tid) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        Node* newNode = new Node(item);   // TODO: replace this with NVM allocator
        PWB(&newNode->item);
        PWB(&newNode->next); // Just in case 'item' and 'next' are not on the same cache line
        while (true) {
            Node* ltail = hp->protectPtr(kHpTail, tail, tid);
            if (ltail == tail.load()) {
                Node* lnext = ltail->next.load();
                if (lnext == nullptr) {
                    PWB(&tail);
                    if (ltail->casNext(nullptr, newNode)) {
                        PWB(&ltail->next);
                        casTail(ltail, newNode);
                        hp->clear(tid);
                        return;
                    }
                } else {
                    PWB(&ltail->next);
                    casTail(ltail, lnext);
                }
            }
        }
    }

    /*
     * Uncontended: at least 2 PWB()s, 1 CAS, and 1 PSYNC()
     */
    T* dequeue(const int tid) {
        Node* node = hp->protect(kHpHead, head, tid);
        while (node != tail.load()) {
            Node* lnext = hp->protect(kHpNext, node->next, tid);
            PWB(&tail);
            PWB(&head);
            if (casHead(node, lnext)) {
                PWB(&head);
                PSYNC();
                T* item = lnext->item;  // Another thread may clean up lnext after we do hp->clear()
                hp->clear(tid);
                hp->retire(node, tid);  // TODO: replace the internal de-allocator with an NVM de-allocator
                return item;
            }
            node = hp->protect(kHpHead, head, tid);
        }
        hp->clear(tid);
        return nullptr;                  // Queue is empty
    }
};

#endif /* _PERSISTENT_MICHAEL_SCOTT_QUEUE_HP_H_ */
