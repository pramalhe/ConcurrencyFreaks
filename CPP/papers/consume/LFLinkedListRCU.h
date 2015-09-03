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

#ifndef _LOCK_FREE_LINKED_LIST_RCU_H_
#define _LOCK_FREE_LINKED_LIST_RCU_H_

#include <atomic>
#include <thread>
#include <set>
#include "RCULRClassic.h"
#include "RIDistributedCacheLineCounter.h"

namespace LockFree {



/**
 * This is a hacked Michael and Scott list
 * <p>
 * In this implementation, we've stripped away the HP related code and use
 * instead RCU.
 * http://???
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class LFLinkedListRCU {

    // Needed outside because of templatization ??
    class Node {
    public:
        T*                 key;
        std::atomic<Node*> next;

        Node(T* lkey) {
            key = lkey;
            next.store(nullptr);
        }

        bool casNext(Node *cmp, Node *val) {
            // Use a tmp variable because this CAS "replaces" the value of the first argument
            Node *tmp = cmp;
            return next.compare_exchange_strong(tmp, val);
        }
    };

public:
    // Pointers to head and tail sentinel nodes of the list
    std::atomic<Node*> _head;
    std::atomic<Node*> _tail;

    RCU::RCULRClassic<RIDistributedCacheLineCounter>  _rcu;

    LFLinkedListRCU() {
        _head.store(new Node(nullptr));
        //_tail.store(new Node(nullptr));
        //_head.load()->next.store(_tail.load());
        _tail.store(_head.load());
    }

    // We don't expect the destructor to be called if this instance can still be in use
    ~LFLinkedListRCU() {
        Node *prev = _head.load();
        Node *node = prev->next.load();
        while (node != nullptr) {
            delete prev;
            prev = node;
            node = prev->next.load();
        }
        delete prev;
    }


    /**
     * This method is named 'Insert()' in the original paper.
     * Taken from Figure 4 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     *
     * @param key
     * @return
     */
    bool add(T *key) {
        if (key == nullptr) return false;
        // We start by allocating the new node, and only then do we
        // set the readersVersion to READING, to reduce waiting time for
        // unlinkAndGC() as much as possible.
        Node *new_node = new Node(key); // TODO: add const if possible
        //const int local_version_index = _rcu->read_lock();
        while (true) {
            Node *t = _tail.load();
            Node *q = t->next.load();
            if (q == nullptr) {
                // It seems this is the last node, so add the newNode here
                // and try to move the tail to the newNode
                if (t->casNext(nullptr, new_node)) {
                    casTail(t, new_node); // Failure is OK.
                    //lr_guard->readIndicatorDepart(local_version_index);
                    return true;
                }
            } else {
                casTail(t, q);
            }
        }
        //_rcu->read_unlock(local_version_index);
    }


    /**
     * This method is named 'Delete()' in the original paper.
     * Progress Condition: Lock-Free
     * TODO:
     */
    bool remove(T key)
    {
        std::cout << "ERROR: remove() is not implemented\n";
        return false;
    }


    /**
     * This is named 'Search()' on the original paper
     * Progress Condition: Lock-Free
     *
     */
    bool contains (T* lookup_key)
    {
        const int lvi = _rcu.read_lock();
        Node *node = _head.load();
        while (node != nullptr) {
            if (node->key != nullptr && *node->key == *lookup_key) {
                _rcu.read_unlock(lvi);
                return true;
            }
            node = node->next.load(std::memory_order_seq_cst);
        }
        _rcu.read_unlock(lvi);
        return false;
    }

    bool casTail(Node *cmp, Node *val) {
        // Use a tmp variable because CAS "replaces" the first argument
        Node *tmp = cmp;
        return _tail.compare_exchange_strong(tmp, val);
    }

};
} // end of namespace
#endif /* _MAGED_M_MICHAEL_LINKED_LIST_RCU_H_ */
