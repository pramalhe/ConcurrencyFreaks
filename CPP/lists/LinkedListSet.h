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


#ifndef _LINKED_LIST_SET_H_
#define _LINKED_LIST_SET_H_


/**
 * A Linked List to be used with other concurrency mechanisms.
 * This data structure is NOT thread-safe
 * <p>
 * This set has three main operations:
 * <ul>
 * <li>add(x)      - Non thread-safe
 * <li>remove(x)   - Non thread-safe
 * <li>contains(x) - Non thread-safe
 * </ul><p>
 * and two helper operations: size() and clear()
 */
template<typename T> class LinkedListSet {

private:
    struct Node {
        T key;
        Node* next;
    };

    Node* _head;
    Node* _tail;

public:

    LinkedListSet() {
        _head = new Node();
        _tail = new Node();
        _head->next = _tail;
    }

    ~LinkedListSet() {
        clear(); // delete all nodes except _head and _tail
        delete _head;
        delete _tail;
    }


    /**
     * Adds a key to the set if it is not already there
     * <p>
     * Progress Condition: non thread-safe
     *
     * @param key
     * @return Returns true if "key" was added to the set, and false if it was
     * already there
     */
    bool add(T key) {
    	Node* newNode = new Node();
        newNode->key = key;
        Node* node = _head->next;
        Node* prev = _head;
        while (node != _tail) {
            if (key == node->key) {
                // This key is already in the set, return false
                return false;
            }
            if (key < node->key) {
                // We found the right place to insert, break out of the while()
                break;
            }
            prev = node;
            node = node->next;
        }
        // Insert the new node
        newNode->next = node;
        prev->next = newNode;
        return true;
    }


    /**
     * Removes a key from the set if it is there
     * <p>
     * Progress Condition: non thread-safe
     *
     * @param key
     * @return Returns true if "key" is found and removed from the list, false
     * if key was not found in the set.
     */
    bool remove(T key) {
        Node* node = _head->next;
        Node* prev = _head;
        while (node != _tail) {
            if (key == node->key) {
                // Found a matching key, unlink the node
                prev->next = node->next;
                delete node;
                return true;
            }
            if (key < node->key) {
                // The key is not in the set, return false
                return false;
            }
            prev = node;
            node = node->next;
        }
        return false;
    }


    /**
     * Searches for a given key.
     * <p>
     * Progress Condition: non thread-safe
     *
     * @param key
     * @return
     */
    bool contains(T key) {
        Node* node = _head->next;
        while (node != _tail) {
            if (key == node->key) {
                // Found the key in the set
                return true;
            }
            if (key < node->key) {
                // The key is not in the set, return false
                return false;
            }
            node = node->next;
        }
        return false;
    }


    /*
     * Cleans all entries in the list (set) except the head and tail
     */
    void clear(void) {
        Node * node = _head->next;
        Node * prev = _head->next;
        while (node != _tail) {
            prev = node;
            node = node->next;
            delete prev;
        }
        _head->next = _tail;
    }


    long size(void) {
        Node * node = _head->next;
        long size = 0;
        while (node != _tail) {
            size++;
            node = node->next;
        }
        return size;
    }
};

#endif /* _LINKED_LIST_SET_H_ */
