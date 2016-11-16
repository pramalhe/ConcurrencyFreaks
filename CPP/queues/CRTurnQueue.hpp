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

#ifndef _CR_TURN_QUEUE_HP_H_
#define _CR_TURN_QUEUE_HP_H_

#include <atomic>
#include <stdexcept>
#include "HazardPointers.hpp"


/**
 * <h1> CR Turn Queue </h1>
 *
 * A concurrent wait-free queue that is Multi-Producer-Multi-Consumer and does
 * its own wait-free memory reclamation.
 * Based on the paper "A Wait-Free Queue with Wait-Free Memory Reclamation"
 * https://github.com/pramalhe/ConcurrencyFreaks/tree/master/papers/crturnqueue-2016.pdf
 *
 * <p>
 * Enqueue algorithm: CR Turn enqueue
 * Dequeue algorithm: CR Turn dequeue
 * Consistency: Linearizable
 * enqueue() progress: wait-free bounded O(N_threads)
 * dequeue() progress: wait-free bounded O(N_threads)
 * Memory Reclamation: Hazard Pointers (wait-free)
 *
 * <p>
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 *
 * @author Andreia Correia
 * @author Pedro Ramalhete
 */
template<typename T>
class CRTurnQueue {

private:
    struct Node {
        T* item;
        const int enqTid;
        std::atomic<int> deqTid;
        std::atomic<Node*> next;

        Node(T* item, int tid) : item{item}, enqTid{tid}, deqTid{IDX_NONE}, next{nullptr} { }

        bool casDeqTid(int cmp, int val) {
     	    return deqTid.compare_exchange_strong(cmp, val);
        }
    };

    static const int IDX_NONE = -1;
    static const int MAX_THREADS = 128;
    const int maxThreads;

    // Pointers to head and tail of the list
    alignas(128) std::atomic<Node*> head;
    alignas(128) std::atomic<Node*> tail;
    // Enqueue requests
    alignas(128) std::atomic<Node*> enqueuers[MAX_THREADS];
    // Dequeue requests
    alignas(128) std::atomic<Node*> deqself[MAX_THREADS];
    alignas(128) std::atomic<Node*> deqhelp[MAX_THREADS];


    HazardPointers<Node> hp {3, maxThreads}; // We need three hazard pointers
    const int kHpTail = 0;
    const int kHpHead = 0;
    const int kHpNext = 1;
    const int kHpDeq = 2;

    Node* sentinelNode = new Node(nullptr, 0);


    /**
     * Called only from dequeue()
     *
     * Search for the next request to dequeue and assign it to lnext.deqTid
     * It is only a request to dequeue if deqself[i] equals deqhelp[i].
     */
    int searchNext(Node* lhead, Node* lnext) {
        const int turn = lhead->deqTid.load();
        for (int idx=turn+1; idx < turn+maxThreads+1; idx++) {
            const int idDeq = idx%maxThreads;
            if (deqself[idDeq].load() != deqhelp[idDeq].load()) continue;
            if (lnext->deqTid.load() == IDX_NONE) lnext->casDeqTid(IDX_NONE, idDeq);
            break;
        }
        return lnext->deqTid.load();
    }


    /**
     * Called only from dequeue()
     *
     * If the ldeqTid is not our own, we must use an HP to protect against
     * deqhelp[ldeqTid] being retired-deleted-newed-reenqueued.
     */
    void casDeqAndHead(Node* lhead, Node* lnext, const int tid) {
        const int ldeqTid = lnext->deqTid.load();
        if (ldeqTid == tid) {
            deqhelp[ldeqTid].store(lnext, std::memory_order_release);
        } else {
            Node* ldeqhelp = hp.protectPtr(kHpDeq, deqhelp[ldeqTid].load(), tid);
            if (ldeqhelp != lnext && lhead == head.load()) {
                deqhelp[ldeqTid].compare_exchange_strong(ldeqhelp, lnext); // Assign next to request
            }
        }
        head.compare_exchange_strong(lhead, lnext);
    }


    /**
     * Called only from dequeue()
     *
     * Giveup procedure, for when there are no nodes left to dequeue
     */
    void giveUp(Node* myReq, const int tid) {
        Node* lhead = head.load();
        if (deqhelp[tid].load() != myReq || lhead == tail.load()) return;
        hp.protectPtr(kHpHead, lhead, tid);
        if (lhead != head.load()) return;
        Node* lnext = hp.protectPtr(kHpNext, lhead->next.load(), tid);
        if (lhead != head.load()) return;
        if (searchNext(lhead, lnext) == IDX_NONE) lnext->casDeqTid(IDX_NONE, tid);
        casDeqAndHead(lhead, lnext, tid);
    }

public:
    CRTurnQueue(int maxThreads=MAX_THREADS) : maxThreads(maxThreads) {
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
        for (int i = 0; i < maxThreads; i++) {
            enqueuers[i].store(nullptr, std::memory_order_relaxed);
            // deqself[i] != deqhelp[i] means that isRequest=false
            deqself[i].store(new Node(nullptr, 0), std::memory_order_relaxed);
            deqhelp[i].store(new Node(nullptr, 0), std::memory_order_relaxed);
        }
    }


    ~CRTurnQueue() {
        delete sentinelNode;
        while (dequeue(0) != nullptr); // Drain the queue
        for (int i=0; i < maxThreads; i++) delete deqself[i].load();
        for (int i=0; i < maxThreads; i++) delete deqhelp[i].load();
    }


    std::string className() { return "CRTurnQueue"; }


    /**
     * Steps when uncontended:
     * 1. Add node to enqueuers[]
     * 2. Insert node in tail.next using a CAS
     * 3. Advance tail to tail.next
     * 4. Remove node from enqueuers[]
     *
     * @param tid The tid must be a UNIQUE index for each thread, in the range 0 to maxThreads-1
     */
    void enqueue(T* item, const int tid) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        Node* myNode = new Node(item,tid);
        enqueuers[tid].store(myNode);
        for (int i = 0; i < maxThreads; i++) {
            if (enqueuers[tid].load() == nullptr) {
                hp.clear(tid);
                return; // Some thread did all the steps
            }
            Node* ltail = hp.protectPtr(kHpTail, tail.load(), tid);
            if (ltail != tail.load()) continue; // If the tail advanced maxThreads times, then my node has been enqueued
            if (enqueuers[ltail->enqTid].load() == ltail) {  // Help a thread do step 4
                Node* tmp = ltail;
                enqueuers[ltail->enqTid].compare_exchange_strong(tmp, nullptr);
            }
            for (int j = 1; j < maxThreads+1; j++) {         // Help a thread do step 2
                Node* nodeToHelp = enqueuers[(j + ltail->enqTid) % maxThreads].load();
                if (nodeToHelp == nullptr) continue;
                Node* nodenull = nullptr;
                ltail->next.compare_exchange_strong(nodenull, nodeToHelp);
                break;
            }
            Node* lnext = ltail->next.load();
     	    if (lnext != nullptr) tail.compare_exchange_strong(ltail, lnext); // Help a thread do step 3
        }
        enqueuers[tid].store(nullptr, std::memory_order_release); // Do step 4, just in case it's not done
        hp.clear(tid);
    }


    /**
     * Steps when uncontended:
     * 1. Publish request to dequeue in dequeuers[tid];
     * 2. CAS node->deqTid from IDX_START to tid;
     * 3. Set dequeuers[tid] to the newly owned node;
     * 4. Advance the head with casHead();
     *
     * We must protect either head or tail with HP before doing the check for
     * empty queue, otherwise we may get into retired-deleted-newed-reenqueued.
     *
     * @param tid: The tid must be a UNIQUE index for each thread, in the range 0 to maxThreads-1
     */
    T* dequeue(const int tid) {
        Node* prReq = deqself[tid].load();     // Previous request
        Node* myReq = deqhelp[tid].load();
        deqself[tid].store(myReq);             // Step 1
        for (int i=0; i < maxThreads; i++) {
            if (deqhelp[tid].load() != myReq) break; // No need for HP
            Node* lhead = hp.protectPtr(kHpHead, head.load(), tid);
            if (lhead != head.load()) continue;
            if (lhead == tail.load()) {        // Give up
                deqself[tid].store(prReq);     // Rollback request to dequeue
                giveUp(myReq, tid);
                if (deqhelp[tid].load() != myReq) {
                    deqself[tid].store(myReq, std::memory_order_relaxed);
                    break;
                }
                hp.clear(tid);
                return nullptr;
            }
            Node* lnext = hp.protectPtr(kHpNext, lhead->next.load(), tid);
            if (lhead != head.load()) continue;
 		    if (searchNext(lhead, lnext) != IDX_NONE) casDeqAndHead(lhead, lnext, tid);
        }
        Node* myNode = deqhelp[tid].load();
        Node* lhead = hp.protectPtr(kHpHead, head.load(), tid);     // Do step 4 if needed
        if (lhead == head.load() && myNode == lhead->next.load()) head.compare_exchange_strong(lhead, myNode);
        hp.clear(tid);
        hp.retire(prReq, tid);
        return myNode->item;
    }

};

#endif /* _CR_TURN_QUEUE_HP_H_ */
