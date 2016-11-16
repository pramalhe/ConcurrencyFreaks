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

#ifndef _LOG2_ARRAY_QUEUE_HP_H_
#define _LOG2_ARRAY_QUEUE_HP_H_

#include <atomic>
#include <stdexcept>
#include "HazardPointers.hpp"


/**
 * <h1> Log 2 Array Queue </h1>
 *
 * Same as Linear Array Queue but does a binary (Log2) search on the array.
 *
 * This is a lock-free queue where each node contains an array of items.
 * Each entry in the array may contain on of three possible values:
 * - A valid item that has been enqueued;
 * - nullptr, which means no item has yet been enqueued in that position;
 * - taken, a special value that means there was an item but it has been dequeued;
 * The enqueue() searches for the first nullptr entry in the array and tries
 * to CAS from nullptr to its item.
 * The dequeue() searches for the first valid item in the array and tries to
 * CAS from item to "taken".
 * The search is done in a binary search which takes at most log2 steps.
 *
 * Enqueue algorithm: Log2 array search with CAS(nullptr,item)
 * Dequeue algorithm: Log2 array search with CAS(item,taken)
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Pointers (lock-free)
 * Uncontended enqueue: 1 CAS + 1 HP
 * Uncontended dequeue: 1 CAS + 1 HP
 *
 * <p>
 * Possible improvements:
 * - We tried to make the indexing write on one cache line at a time
 *  (see getIndex()) but it didn't seem to make a difference. More research
 *  into this may be interesting;
 * - Create "nextEnq" and "nextDeq" atomic variables per node where each
 *   enqueuer and dequeuer write in the end of their operation the index
 *   of the next nullptr/item. This may be overwritten by an older thread
 *   but remember that this is per-thread so it may pay off.
 *   The store can be memory_order_release.
 *
 *
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
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class Log2ArrayQueue {
    static const long BUFFER_SIZE = 1024; // 16, 32, 128, 1024, or 2^17 for LCRQ

private:
    struct Node {
        std::atomic<T*> items[BUFFER_SIZE];
        std::atomic<Node*> next;

        Node(T* item) : next{nullptr} {
            items[0].store(item, std::memory_order_relaxed);
            for (long i = 1; i < BUFFER_SIZE; i++) {
                items[i].store(nullptr, std::memory_order_relaxed);
            }
        }

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

    // Pointers to head and tail of the list
    std::atomic<Node*> head alignas(128);
    std::atomic<Node*> tail alignas(128);

    static const int MAX_THREADS = 128;
    const int maxThreads;

    T* taken = (T*)new int();  // Muuuahahah !

    // We need just one hazard pointer
    HazardPointers<Node> hp {1, maxThreads};
    const int kHpTail = 0;
    const int kHpHead = 0;


    long getIndex(long i) {
        return i;
        //return ((i << 4) % BUFFER_SIZE) + (i >> (10-4)); // 4 is for cache padding
    }

    long findFirstNull(Node* node) {
        if (node->items[0].load() == nullptr) return 0;
        long minPos = 0;
        long maxPos = BUFFER_SIZE-1;
        while (true) {
            long pos = (maxPos-minPos)/2 + minPos;
            if (node->items[getIndex(pos)].load() == nullptr) {
                maxPos = pos;
            } else {
                minPos = pos;
            }
            if (maxPos-minPos <= 3) return minPos;
        }
    }

    long findLastTaken(Node* node) {
        if (node->items[BUFFER_SIZE-1].load() == taken) return BUFFER_SIZE-1;
        long minPos = 0;
        long maxPos = BUFFER_SIZE-1;
        while (true) {
            long pos = (maxPos-minPos)/2 + minPos;
            if (node->items[getIndex(pos)].load() == taken) {
                minPos = pos;
            } else {
                maxPos = pos;
            }
            if (maxPos-minPos <= 3) return minPos;
        }
    }

public:
    Log2ArrayQueue(int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        Node* sentinelNode = new Node(nullptr);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
    }


    ~Log2ArrayQueue() {
        while (dequeue(0) != nullptr); // Drain the queue
        delete head.load();            // Delete the last node
        delete (int*)taken;
    }


    std::string className() { return "Log2ArrayQueue"; }


    void enqueue(T* item, const int tid) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        while (true) {
            Node* ltail = hp.protect(kHpTail, tail, tid);
            if (ltail->items[getIndex(BUFFER_SIZE-1)].load() != nullptr) { // This node is full
                if (ltail != tail.load()) continue;
                Node* lnext = ltail->next.load();
                if (lnext == nullptr) {
                    Node* newNode = new Node(item);
                    if (ltail->casNext(nullptr, newNode)) {
                        casTail(ltail, newNode);
                        hp.clear(tid);
                        return;
                    }
                    delete newNode;
                } else {
                    casTail(ltail, lnext);
                }
                continue;
            }
            // Find the first null entry in items[] and try to CAS from null to item
            for (long i = findFirstNull(ltail); i < BUFFER_SIZE; i++) {
                if (ltail->items[getIndex(i)].load() != nullptr) continue;
                T* itemnull = nullptr;
                if (ltail->items[getIndex(i)].compare_exchange_strong(itemnull, item)) {
                    hp.clear(tid);
                    return;
                }
                if (ltail != tail.load()) break;
            }
        }
    }


    T* dequeue(const int tid) {
        while (true) {
            Node* lhead = hp.protect(kHpHead, head, tid);
            if (lhead->items[getIndex(BUFFER_SIZE-1)].load() == taken) { // This node has been drained, check if there is another one
                Node* lnext = lhead->next.load();
                if (lnext == nullptr) { // No more nodes in the queue
                    hp.clear(tid);
                    return nullptr;
                }
                if (casHead(lhead, lnext)) hp.retire(lhead, tid);
                continue;
            }
            // Find the first non taken entry in items[] and try to CAS from item to taken
            long i = findLastTaken(lhead);
            for (; i < BUFFER_SIZE; i++) {
                T* item = lhead->items[getIndex(i)].load();
                if (item == nullptr) {
                    hp.clear(tid);
                    return nullptr;            // This node is empty
                }
                if (item == taken) continue;
                if (lhead->items[getIndex(i)].compare_exchange_strong(item, taken)) {
                    hp.clear(tid);
                    return item;
                }
                if (lhead != head.load()) break;
            }
        }
    }
};

#endif /* _LOG2_ARRAY_QUEUE_HP_H_ */
