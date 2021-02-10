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

#ifndef _LEFT_RIGHT_ORDERED_LINKED_LIST_SINGLE_H_
#define _LEFT_RIGHT_ORDERED_LINKED_LIST_SINGLE_H_

#include <atomic>
#include <mutex>
#include <thread>
#include <pthread.h>

// Cache line optimization constants
#define LROLLS_CACHE_LINE          64               // Size in bytes of a cache line
#define LROLLS_CACHE_PADD          (LROLLS_CACHE_LINE-sizeof(std::atomic<int>))
#define LROLLS_NUMBER_OF_CORES     32
#define LROLLS_HASH_RATIO          3
#define LROLLS_COUNTERS_RATIO      (LROLLS_HASH_RATIO*LROLLS_CACHE_LINE/sizeof(int))


/**
 * Left-Right Ordered Linked List
 *
 * This linked list data structure implements a concurrent ordered set with
 * the following operations and respective progress conditions:
 * add()      - Blocking
 * remove()   - Blocking
 * contains() - Wait-Free Populations Oblivious
 * <p>
 * We used the Left-Right pattern described in
 * http://sourceforge.net/projects/ccfreaks/files/papers/LeftRight/leftright-extended.pdf
 * <p>
 * Memory usage per key: One "Node" class with three pointers
 * 32 bit machine: 4 x 3 = 12 bytes per key
 * 64 bit machine: 8 x 3 = 24 bytes per key
 * <p>
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class LROrderedLinkedListSingle {

public:
    const int READS_ON_LEFT  = 0;
    const int READS_ON_RIGHT = 1;

    struct Node {
    	T key;
    	Node* nextLeft;
    	Node* nextRight;
    };

    // On the LR pattern we usually need two instance, but in this variant we
    // have a single instance, and therefore, we need a single head and tail
    Node* _head;
    Node* _tail;

    // Members used for the Left-Right mechanism
    std::atomic<int> _leftRight;
    std::atomic<int> _versionIndex;
    std::atomic<int> *_readersVersion0;
    std::atomic<int> *_readersVersion1;
    std::mutex _writersMutex;
    /* Number of cores on the system */
    int          _numCores;
    /* Length of readersVersion0/1[] */
    int          _readersLength;


public:
    LROrderedLinkedListSingle() {
        _head = new Node();
        _tail = new Node();
        _head->nextLeft = _tail;
        _head->nextRight = _tail;
        _leftRight = READS_ON_LEFT;
        _versionIndex = 0;
        _readersVersion0 = 0;
        _readersVersion1 = 0;
        int hw_cores = std::thread::hardware_concurrency();
        if (hw_cores == 0) hw_cores = LROLLS_NUMBER_OF_CORES;
        _numCores = hw_cores;
        _readersLength = _numCores*LROLLS_COUNTERS_RATIO;
        // Allocate and initialize the readIndicators for V0 and V1
        _readersVersion0 = new std::atomic<int>[_readersLength];
        _readersVersion1 = new std::atomic<int>[_readersLength];
        for (int tid = 0; tid < _readersLength; tid += LROLLS_COUNTERS_RATIO) {
            _readersVersion0[tid] = 0;
            _readersVersion1[tid] = 0;
        }
    }


    ~LROrderedLinkedListSingle() {
        Node* node = _head;
        while (node->nextLeft != _tail) {
            _head = node->nextLeft;
            delete node;
            node = _head;
        }
    	delete _head;
    	delete _tail;
    	delete[] _readersVersion0;
    	delete[] _readersVersion1;
    }

private:

    int thread_2_tid (void) {
        std::hash<std::thread::id> hashFunc;
        std::size_t tid = hashFunc(std::this_thread::get_id());
        tid ^= (tid << 21);
        tid ^= (tid >> 35);
        tid ^= (tid << 4);
        return (int)((tid % _numCores)*LROLLS_COUNTERS_RATIO);
    }

    /**
     * Marks that a new Reader has arrived at the readIndicator.
     *
     * Progress Condition: Wait-Free Population Oblivious (on x86)
     *
     * @return the current versionIndex
     */
    int readIndicatorArrive(const int tid) {
    	int localVersionIndex = _versionIndex.load();
    	if (localVersionIndex == 0) {
    	    _readersVersion0[tid].fetch_add(1);
        } else {
    	    _readersVersion1[tid].fetch_add(1);
        }
    	return localVersionIndex;
    }

    /**
     * Marks that a Reader has departed from the readIndicator.
     *
     * Progress Condition: Wait-Free Population Oblivious (on x86)
     *
     * @param localVersionIndex
     */
    void readIndicatorDepart(const int tid, const int localVersionIndex) {
        if (localVersionIndex == 0) {
            _readersVersion0[tid].fetch_add(-1);
        } else {
            _readersVersion1[tid].fetch_add(-1);
        }
    }

    /**
     * Checks if there are Readers in a given readIndicator.
     *
     * Progress Condition: Wait-Free Population Oblivious
     *
     * @return true if the counters are at zero
     */
    bool readIndicatorIsEmpty(const int localVersionIndex) {
        if (localVersionIndex == 0) {
            for (int tid = 0; tid < _readersLength; tid += LROLLS_COUNTERS_RATIO) {
                if (_readersVersion0[tid].load() != 0) return false;
            }
        } else {
            for (int tid = 0; tid < _readersLength; tid += LROLLS_COUNTERS_RATIO) {
                if (_readersVersion1[tid].load() != 0) return false;
            }
        }
        return true;
    }


    /**
     * Waits for all the threads doing a "Read" to finish their tasks on the
     * TreeMap that the "Write" wants to modify.
     * Must be called only by "Write" operations, and it {@code writersMutex}
     * must be locked when this function is called.
     *
     * Progress Condition: Blocking
     */
    void toggleVersionAndWait(void) {
    	const int localVersionIndex = _versionIndex.load();
    	const int prevVersionIndex = (localVersionIndex % 2);
    	const int nextVersionIndex = ((localVersionIndex+1) % 2);

    	// Wait for Readers from next version
    	while (!readIndicatorIsEmpty(nextVersionIndex)) {
    		std::this_thread::yield();
    	}

    	// Toggle the versionIndex variable
    	_versionIndex.store(nextVersionIndex);

    	// Wait for Readers from previous version
    	while (!readIndicatorIsEmpty(prevVersionIndex)) {
    		std::this_thread::yield();
    	}
    }


    /**
     * Finds a node with a matching key, and returns the previous one.
     * Operates on the logical left list.
     *
     * Progress Condition: Wait-Free Population Oblivious
     *
     * @return The node previous to the one with a matching key, or tail
     */
    Node* findPrevOnLeft(T key) {
        Node* node = _head->nextLeft;
        Node* prev = _head;
        while (node != _tail) {
            if (key == node->key || key < node->key) return prev;
            prev = node;
            node = node->nextLeft;
        }
        return prev;
    }


    /**
     * Finds a node with a matching key, and returns the previous one.
     * Operates on the logical right list.
     *
     * Progress Condition: Wait-Free Population Oblivious
     *
     * @return The node previous to the one with a matching key, or tail
     */
    Node* findPrevOnRight(T key) {
        Node* node = _head->nextRight;
        Node* prev = _head;
        while (node != _tail) {
            if (key == node->key || key < node->key) return prev;
            prev = node;
            node = node->nextRight;
        }
        return prev;
    }


public:
    /**
     * Inserts a key in the set
     *
     * Progress Condition: Blocking
     *
     * @param  key
     * @return Returns true if the key was added to the list. Returns false if
     *         the key is already in the set.
     */
    bool add(T key) {
        Node* prevNode = nullptr;

        std::lock_guard<std::mutex> lock(_writersMutex);
        // Insert the node immediately on the unused logical list
        if (_leftRight.load() == READS_ON_LEFT) {
            prevNode = findPrevOnRight(key);
            if (prevNode->nextRight != _tail && prevNode->nextRight->key == key) {
                return false;
            }
            Node* newNode = new Node();
            newNode->key = key;
            newNode->nextRight  = prevNode->nextRight;
            newNode->nextLeft   = prevNode->nextLeft;
            prevNode->nextRight = newNode;
            _leftRight.store(READS_ON_RIGHT);
            toggleVersionAndWait();
            prevNode->nextLeft = newNode;
        } else {
            prevNode = findPrevOnLeft(key);
            if (prevNode->nextLeft != _tail && prevNode->nextLeft->key == key) {
                return false;
            }
            Node* newNode = new Node();
            newNode->key = key;
            newNode->nextRight = prevNode->nextRight;
            newNode->nextLeft  = prevNode->nextLeft;
            prevNode->nextLeft = newNode;
            _leftRight.store(READS_ON_LEFT);
            toggleVersionAndWait();
            prevNode->nextRight = newNode;
        }
        return true;
    }


    /**
     * Checks if a key is in the set.
     *
     * Progress Condition: Wait-Free Population Oblivious
     *
     * @param key
     * @return Returns true if the key is present in the set and false otherwise
     */
    bool contains(T key) {
    	Node* foundNode = nullptr;
    	Node* prevNode = nullptr;
        const int tid = thread_2_tid();
    	const int localVersionIndex = readIndicatorArrive(tid);

        if (_leftRight.load() == READS_ON_LEFT) {
            foundNode = findPrevOnLeft(key)->nextLeft;
        } else {
            foundNode = findPrevOnRight(key)->nextRight;
        }

        const bool retValue = (foundNode->key == key);
        readIndicatorDepart(tid, localVersionIndex);
        return retValue;
    }


    /**
     * Removes a key from the set.
     *
     * Progress Condition: Blocking
     *
     * @param key
     * @return True if the key was successfuly removed and false if there was
     *         no matching key in the set.
     */
    bool remove(T key) {
        Node* prevNode = nullptr;
        Node* rmNode = nullptr;

        std::lock_guard<std::mutex> lock(_writersMutex);
        // Remove the node immediately on the unused logical list
        if (_leftRight.load() == READS_ON_LEFT) {
        	prevNode = findPrevOnRight(key);
        	rmNode = prevNode->nextRight;
        	// findPrevOnRight() will return the nearest previous node if there is no match
        	if (rmNode == _tail || rmNode->key != key) {
        		return false;
        	}
        	prevNode->nextRight = rmNode->nextRight;
        	_leftRight.store(READS_ON_RIGHT);
        	toggleVersionAndWait();
            prevNode->nextLeft = rmNode->nextRight;
        } else {
        	prevNode = findPrevOnLeft(key);
        	rmNode = prevNode->nextLeft;
        	// findPrevOnLeft() will return the nearest previous node if there is no match
        	if (rmNode == _tail || rmNode->key != key) {
        	    return false;
        	}
        	prevNode->nextLeft = rmNode->nextLeft;
        	_leftRight.store(READS_ON_LEFT);
        	toggleVersionAndWait();
            prevNode->nextRight = rmNode->nextLeft;
        }
        delete rmNode;
        return true;
    }

};

#endif /* _LEFT_RIGHT_ORDERED_LINKED_LIST_SINGLE_H_ */
