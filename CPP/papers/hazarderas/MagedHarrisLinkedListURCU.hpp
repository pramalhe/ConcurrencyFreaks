/******************************************************************************
 * Copyright (c) 2016, Pedro Ramalhete, Andreia Correia
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

#ifndef _MAGED_MICHAEL_TIM_HARRIS_LINKED_LIST_URCU_H_
#define _MAGED_MICHAEL_TIM_HARRIS_LINKED_LIST_URCU_H_

#include <atomic>
#include <thread>
#include <iostream>
#include <string>
#include <vector>
#include "URCUGraceVersion.hpp"



/**
 * This is the linked list by Maged M. Michael but we modified it to use URCU.
 * Lock-Free Linked List as described in Maged M. Michael paper (Figure 4):
 * http://www.cs.tau.ac.il/~afek/p73-Lock-Free-HashTbls-michael.pdf
 * We had to add a list of retired nodes for the code path where the find()
 * does the removal which complicates the code a bit, but we didn't want to
 * re-write it.
 *
 * The URCU variant is the one in our Grace Sharing URCU paper.
 *
 * We can have wait-free contains() due to using URCU, so we changed contains()
 * to traverse the list without helping, i.e. without calling find()
 *
 * 
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Lock-Free
 * <li>remove(x)   - Lock-Free (blocking due to calling synchronize_rcu() of URCU)
 * <li>contains(x) - Lock-Free
 * </ul><p>
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T> 
class MagedHarrisLinkedListURCU {

private:
    struct Node {
        T* key;
        std::atomic<Node*> next;

        Node(T* key) : key{key}, next{nullptr} { }

        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    // Pointers to head and tail sentinel nodes of the list
    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    const int maxThreads;

    URCUGraceVersion urcu {maxThreads};

public:

    MagedHarrisLinkedListURCU(const int maxThreads) : maxThreads{maxThreads} {
        head.store(new Node(nullptr));
        tail.store(new Node(nullptr));
        head.load()->next.store(tail.load());
    }


    // We don't expect the destructor to be called if this instance can still be in use
    ~MagedHarrisLinkedListURCU() {
        Node *prev = head.load();
        Node *node = prev->next.load();
        while (node != nullptr) {
            delete prev;
            prev = node;
            node = prev->next.load();
        }
        delete prev;
    }

    std::string className() { return "MagedHarrisLinkedListURCU"; }


    /**
     * This method is named 'Insert()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     *
     */
    bool add(T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        std::vector<Node*> retired;
        Node* newNode = new Node(key);
        urcu.read_lock(tid);
        while (true) {
            if (find(key, &prev, &curr, &next, retired)) {
                delete newNode;              // There is already a matching key
                urcu.read_unlock(tid);
                deleteRetired(retired);
                return false;
            }
            newNode->next.store(curr, std::memory_order_relaxed);
            Node *tmp = getUnmarked(curr);
            if (prev->compare_exchange_strong(tmp, newNode)) { // seq-cst
                urcu.read_unlock(tid);
                deleteRetired(retired);
                return true;
            }
        }
    }


    /**
     * This method is named 'Delete()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     */
    bool remove(T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        std::vector<Node*> retired;
        urcu.read_lock(tid);
        while (true) {
            /* Try to find the key in the list. */
            if (!find(key, &prev, &curr, &next, retired)) {
                urcu.read_unlock(tid);
                deleteRetired(retired);
                return false;
            }
            /* Mark if needed. */
            Node *tmp = getUnmarked(next);
            if (!curr->next.compare_exchange_strong(tmp, getMarked(next))) {
                continue; /* Another thread interfered. */
            }

            tmp = getUnmarked(curr);
            if (prev->compare_exchange_strong(tmp, getUnmarked(next))) { /* Unlink */
                urcu.read_unlock(tid);
                urcu.synchronize_rcu();
                delete getUnmarked(curr);
            } else {
                urcu.read_unlock(tid);
            }
            deleteRetired(retired);
            /*
             * If we want to prevent the possibility of there being an
             * unbounded number of unmarked nodes, add "else _find(head,key)."
             * This is not necessary for correctness.
             */
            return true;
        }
    }


    /**
     * This is named 'Search()' on the original paper
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     */
    bool contains (T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        std::vector<Node*> retired;
        urcu.read_lock(tid);
        bool isContains = find(key, &prev, &curr, &next, retired);
        urcu.read_unlock(tid);
        deleteRetired(retired);
        return isContains;
    }


    /**
     * Wait-free bounded implementation
     */
    /*
    bool contains (T* key)
    {
        const int mytid = urcu.allocTID();
        urcu.read_lock(mytid);
        Node* curr = head.load()->next.load(std::memory_order_acquire);
        bool isContains = false;
        while (curr != nullptr) {
            if (curr->key != nullptr && *curr->key < *key) {
                curr = curr->next.load(std::memory_order_acquire);
                continue;
            }
            if (*curr->key == *key) isContains = true; // TODO: do we need to check for marked?
            break;
        }
        urcu.read_unlock(mytid);
        return isContains;
    }
    */


private:

    /**
     * <p>
     * Progress Condition: Lock-Free
     */
    bool find (T* key, std::atomic<Node*> **par_prev, Node **par_curr, Node **par_next, std::vector<Node*>& retired)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next;

     try_again:
        prev = &head;
        curr = prev->load();
        if (prev->load() != getUnmarked(curr)) goto try_again;
        while (true) {
            if (getUnmarked(curr) == nullptr) break;
            next = curr->next.load();
            if (getUnmarked(next) == tail.load()) break;
            if (getUnmarked(curr)->next.load() != next) goto try_again;
            if (prev->load() != getUnmarked(curr)) goto try_again;
            if (getUnmarked(next) == next) { // !cmark in the paper
                if (getUnmarked(curr)->key != nullptr && !(*getUnmarked(curr)->key < *key)) { // Check for null to handle head and tail
                    *par_curr = curr;
                    *par_prev = prev;
                    *par_next = next;
                    return (*getUnmarked(curr)->key == *key);
                }
                prev = &getUnmarked(curr)->next;
            } else {
                // Update the link and retire the node.
                Node *tmp = getUnmarked(curr);
                if (!prev->compare_exchange_strong(tmp, getUnmarked(next))) {
                    goto try_again;
                }
                // We can't delete the nodes here, got to wait for rcu_read_unlock()
                retired.push_back(getUnmarked(curr));
            }
            curr = next;
        }
        *par_curr = curr;
        *par_prev = prev;
        *par_next = next;
        return false;
    }

    bool isMarked(Node * node) {
    	return ((size_t) node & 0x1);
    }

    Node * getMarked(Node * node) {
    	return (Node*)((size_t) node | 0x1);
    }

    Node * getUnmarked(Node * node) {
    	return (Node*)((size_t) node & (~0x1));
    }

    void deleteRetired(std::vector<Node*>& retired) {
        if (retired.size() > 0) {
            urcu.synchronize_rcu();
            for (auto retNode : retired) delete retNode;
        }
    }
};

#endif /* _MAGED_MICHAEL_TIM_HARRIS_LINKED_LIST_URCU_H_ */
