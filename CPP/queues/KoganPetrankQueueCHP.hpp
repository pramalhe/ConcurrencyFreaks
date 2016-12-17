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

#ifndef _KOGAN_PETRANK_QUEUE_CHP_H_
#define _KOGAN_PETRANK_QUEUE_CHP_H_

#include <atomic>
#include <stdexcept>
#include "HazardPointers.hpp"
#include "HazardPointersConditional.hpp"


/**
 * <h1> Kogan-Petrank Queue with Conditional Hazard Pointers </h1>
 *
 * http://www.cs.technion.ac.il/~erez/Papers/wfquque-ppopp.pdf
 *
 * enqueue algorithm: Kogan-Petrank, based on the consensus of Lamport's bakery
 * dequeue algorithm: Kogan-Petrank, based on the consensus of Lamport's bakery
 * Consistency: Linearizable
 * enqueue() progress: wait-free bounded O(N_threads)
 * dequeue() progress: wait-free bounded O(N_threads)
 * Memory Reclamation: Hazard Pointers + Hazard Pointers Conditional
 *
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 *
 */
template<typename T>
class KoganPetrankQueueCHP {

private:

    struct Node {
        std::atomic<T*> item;
        const int enqTid;
        std::atomic<int> deqTid { IDX_NONE };
        std::atomic<Node*> next { nullptr };

        Node(T* userItem, int enqTid) : item{userItem}, enqTid{enqTid} { }

        bool casNext(Node* cmp, Node* val) {
            // Use a tmp variable because this CAS "replaces" the value of the first argument
            Node* tmp = cmp;
            return next.compare_exchange_strong(tmp, val);
        }
    };


    struct OpDesc {
        const long long phase;
        const bool pending;
        const bool enqueue;
        Node* node; // This is immutable once assigned
        OpDesc (long long ph, bool pend, bool enq, Node* n) : phase{ph}, pending{pend}, enqueue{enq}, node{n} { }
    };


    bool casTail(Node *cmp, Node *val) {
        return tail.compare_exchange_strong(cmp, val);
    }

    bool casHead(Node *cmp, Node *val) {
        return head.compare_exchange_strong(cmp, val);
    }

    // Member variables
    static const int MAX_THREADS = 128;

    // Pointers to head and tail of the list
    alignas(128) std::atomic<Node*> head;
    alignas(128) std::atomic<Node*> tail;
    // Array of enque and dequeue requests
    alignas(128) std::atomic<OpDesc*> state[MAX_THREADS];

    const static int IDX_NONE = -1;
    OpDesc* OPDESC_END = new OpDesc(IDX_NONE,  false, true, nullptr);
    const int maxThreads;

    const static int HP_CRT_REQ = 3;

    // Hazard Pointers and HPC
    HazardPointers<OpDesc> hpOpDesc {2, maxThreads}; // We only need two HPs for OpDesc instances
    const int kHpODCurr = 0;
    const int kHpODNext = 1;
    HazardPointersConditional<Node> hpNode {3, maxThreads}; // This will delete only if Node.item == nullptr
    const int kHpCurr = 0;
    const int kHpNext = 1;
    const int kHpPrev = 2;


public:
    KoganPetrankQueueCHP(int maxThreads=MAX_THREADS) : maxThreads(maxThreads) {
        Node* sentinelNode = new Node(nullptr, IDX_NONE);
        head.store(sentinelNode);
        tail.store(sentinelNode);
        for (int i = 0; i < maxThreads; i++) {
            state[i].store(OPDESC_END);
        }
    }

    ~KoganPetrankQueueCHP() {
        while (dequeue(0) != nullptr); // Drain the queue
        delete head.load(); // Delete the last node
        delete OPDESC_END;
    }

    std::string className() { return "KoganPetrankQueueCHP"; }


    void help(long long phase, const int TID)
    {
        for (int i = 0; i < maxThreads; i++) {
            // Try to validate the HP for OpDesc at most MAX_OPDESC_TRANS times
            OpDesc* desc = hpOpDesc.protectPtr(kHpODCurr, state[i].load(), TID);
            int it = 0;
            for (; it < maxThreads+1; it++) {
                if (desc == state[i].load()) break;
                desc = hpOpDesc.protectPtr(kHpODCurr, state[i].load(), TID);
            }
            if (it == maxThreads+1 && desc != state[i].load()) continue;
            if (desc->pending && desc->phase <= phase) {
            	if (desc->enqueue) {
            		help_enq(i, phase, TID);
            	} else {
            		help_deq(i, phase, TID);
            	}
            }
        }
    }


    /**
     * Progress Condition: wait-free bounded by maxThreads
     */
    long long maxPhase(const int TID) {
        long long maxPhase = -1;
        for (int i = 0; i < maxThreads; i++) {
            // Try to validate the HP for OpDesc at most MAX_OPDESC_TRANS times
            OpDesc* desc = hpOpDesc.protectPtr(kHpODCurr, state[i].load(), TID);
            int it = 0;
            for (; it < maxThreads+1; it++) {
                if (desc == state[i].load()) break;
                desc = hpOpDesc.protectPtr(kHpODCurr, state[i].load(), TID);
            }
            if (it == maxThreads+1 && desc != state[i].load()) continue;
            long long phase = desc->phase;
            if (phase > maxPhase) {
            	maxPhase = phase;
            }
        }
        return maxPhase;
    }


    bool isStillPending(int tid, long long ph, const int TID) {
        OpDesc* desc = hpOpDesc.protectPtr(kHpODNext, state[tid].load(), TID);
        int it = 0;
        for (; it < maxThreads+1; it++) {
            if (desc == state[tid].load()) break;
            desc = hpOpDesc.protectPtr(kHpODNext, state[tid].load(), TID);
        }
        if (it == maxThreads+1 && desc != state[tid].load()) return false;
        return desc->pending && desc->phase <= ph;
    }


    void enqueue(T* item, const int TID) {
        // We better have consecutive thread ids, otherwise this will blow up
        long long phase = maxPhase(TID) + 1;
        state[TID].store(new OpDesc(phase, true, true, new Node(item, TID)));
        help(phase, TID);
        help_finish_enq(TID);
        hpOpDesc.clear(TID);
        hpNode.clear(TID);
        OpDesc* desc = state[TID].load();
        for (int i = 0; i < maxThreads*2; i++) { // Is maxThreads+1 enough?
            if (desc == OPDESC_END) break;
            if (state[TID].compare_exchange_strong(desc, OPDESC_END)) break;
            desc = state[TID].load();
        }
        hpOpDesc.retire(desc, TID);
    }


    void help_enq(int tid, long long phase, const int TID) {
        while (isStillPending(tid, phase, TID)) {
            Node* last = hpNode.protectPtr(kHpCurr, tail.load(), TID);
            if (last != tail.load()) continue;
            Node* next = last->next.load();
            if (last == tail) {
                if (next == nullptr) {
                    if (isStillPending(tid, phase, TID)) {
                        OpDesc* curDesc = hpOpDesc.protectPtr(kHpODCurr, state[tid].load(), TID);
                        if (curDesc != state[tid].load()) continue;
                        if (last->casNext(next, curDesc->node)) {
                            help_finish_enq(TID);
                            return;
                        }
                    }
                } else {
                    help_finish_enq(TID);
                }
            }
        }
    }


    void help_finish_enq(const int TID) {
        Node* last = hpNode.protectPtr(kHpCurr, tail.load(), TID);
        if (last != tail.load()) return;
        // The inner loop will run at most twice, because last->next is immutable when non-null
        Node* next = hpNode.protect(kHpNext, last->next, TID);
        // Check "last" equals "tail" to prevent ABA on "last->next"
        if (last == tail && next != nullptr) {
            int tid = next->enqTid;
            OpDesc* curDesc = hpOpDesc.protectPtr(kHpODCurr, state[tid], TID);
            if (curDesc != state[tid].load()) return;
            if (last == tail && curDesc->node == next) {
            	OpDesc* newDesc = new OpDesc(curDesc->phase, false, true, next);
            	OpDesc* tmp = curDesc;
            	if(state[tid].compare_exchange_strong(tmp, newDesc)){
            		hpOpDesc.retire(curDesc, TID);
            	} else {
            		delete newDesc;
            	}
            	casTail(last, next);
            }
        }
    }


    T* dequeue(const int TID) {
        // We better have consecutive thread ids, otherwise this will blow up
        long long phase = maxPhase(TID) + 1;
        state[TID].store(new OpDesc(phase, true, false, nullptr));
        help(phase, TID);
        help_finish_deq(TID);
        OpDesc* curDesc = hpOpDesc.protect(kHpODCurr, state[TID], TID);
        Node* node = curDesc->node; // No need for hp because this thread will be the one to retire "node"
        if (node == nullptr) {
            hpOpDesc.clear(TID);
            hpNode.clear(TID);
            OpDesc* desc = state[TID].load();
            for (int i = 0; i < MAX_THREADS; i++) {
                if (state[TID].compare_exchange_strong(desc, OPDESC_END)) break;
                desc = state[TID].load();
                if (desc == OPDESC_END) break;
            }
            hpOpDesc.retire(desc, TID);
            return nullptr; // We return null instead of throwing an exception
        }
        Node* next = node->next; // No need for chp because "next" can only be deleted when item set to nullptr
        T* value = next->item.load();
        next->item.store(nullptr); // "next" can be deleted now
        hpOpDesc.clear(TID);
        hpNode.clear(TID);
        hpNode.retire(node, TID); // "node" will be deleted only when node.item == nullptr
        OpDesc* desc = state[TID].load();
        for (int i = 0; i < maxThreads*2; i++) { // Is maxThreads+1 enough?
            if (desc == OPDESC_END) break;
            if (state[TID].compare_exchange_strong(desc, OPDESC_END)) break;
            desc = state[TID].load();
        }
        hpOpDesc.retire(desc, TID);
        return value;
    }


    void help_deq(int tid, long long phase, const int TID) {
        while (isStillPending(tid, phase, TID)) {
            Node* first = hpNode.protectPtr(kHpPrev, head, TID);
            Node* last = hpNode.protectPtr(kHpCurr, tail, TID);
            if (first != head.load() || last != tail.load()) continue;
            Node* next = first->next.load();
            if (first == head) {
            	if (first == last) {
            		if (next == nullptr) {
            			OpDesc* curDesc = hpOpDesc.protectPtr(kHpODCurr, state[tid], TID);
            			if (curDesc != state[tid].load()) continue;
            			if (last == tail && isStillPending(tid, phase, TID)) {
            			    OpDesc* newDesc = new OpDesc(curDesc->phase, false, false, nullptr);
            			    OpDesc* tmp = curDesc;
                            if (state[tid].compare_exchange_strong(tmp, newDesc)) {
                                hpOpDesc.retire(curDesc, TID);
                            } else {
                                delete newDesc;
                            }
            			}
                    } else {
                        help_finish_enq(TID);
                    }
                } else {
                    OpDesc* curDesc = hpOpDesc.protectPtr(kHpODCurr, state[tid], TID);
                    if (curDesc != state[tid].load()) continue;
                    Node* node = curDesc->node;
                    if (!isStillPending(tid, phase, TID)) break;
                    if (first == head && node != first) {
                        OpDesc* newDesc = new OpDesc(curDesc->phase, true, false, first);
                        OpDesc* tmp = curDesc;
                        if (state[tid].compare_exchange_strong(tmp, newDesc)) {
                            hpOpDesc.retire(curDesc, TID);
                        } else {
                            delete newDesc;
                            continue;
                        }
                    }
                    int tmp = -1;
                    first->deqTid.compare_exchange_strong(tmp, tid);
                    help_finish_deq(TID);
                }
            }
        }
    }


    void help_finish_deq(const int TID) {
        Node* first = hpNode.protectPtr(kHpPrev, head, TID);
        if (first != head.load()) return;
        Node* next = first->next.load();
        int tid = first->deqTid.load();
        if (tid != -1) {
            OpDesc* curDesc = nullptr;
            for (int i = 0; i < MAX_THREADS; i++) {
                curDesc = hpOpDesc.protectPtr(kHpODCurr, state[tid], TID);
                if (curDesc == state[tid].load()) break;
                if (i == MAX_THREADS-1) return; // If the opdesc has changed these many times, the operation must be complete
            }
            if (first == head && next != nullptr) {
            	OpDesc* newDesc = new OpDesc(curDesc->phase, false, false, curDesc->node);
            	OpDesc* tmp = curDesc;
            	if (state[tid].compare_exchange_strong(tmp, newDesc)) {
                    hpOpDesc.retire(curDesc, TID);
            	} else {
                    delete newDesc;
                }
            	casHead(first, next);
            }
        }
    }
};

#endif /* _KOGAN_PETRANK_QUEUE_HP_H_ */
