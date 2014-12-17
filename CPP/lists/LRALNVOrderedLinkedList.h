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

#ifndef _LEFT_RIGHT_ATOMIC_LONG_NO_VERSION_ORDERED_LINKED_LIST_H_
#define _LEFT_RIGHT_ATOMIC_LONG_NO_VERSION_ORDERED_LINKED_LIST_H_

#include "LeftRightALNV.h"
#include "LinkedListSet.h"


/**
 * Left-Right Atomic Long No Version Ordered Linked List
 *
 * This linked list data structure implements a concurrent ordered set with
 * the following operations and respective progress conditions:
 * add()      - Blocking
 * remove()   - Blocking
 * contains() - Wait-Free Populations Oblivious
 * <p>
 * We used the Left-Right Atomic Long No Version variant
 * The Left-Right pattern is described here:
 * http://concurrencyfreaks.com/2013/12/left-right-concurrency-control.html
 * The Atomic Long variant is described on this post:
 * http:...
 * <p>
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class LRALNVOrderedLinkedList {

    LeftRight::LeftRightALNV<LinkedListSet<T>*> _lralnv;
    LinkedListSet<T> _setL;
    LinkedListSet<T> _setR;


public:
    LRALNVOrderedLinkedList() {
    }

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
        _lralnv.writersLock();
        // Insert the node immediately on the unused list
        if (_lralnv.currentLeftRight() == LeftRight::READS_ON_LEFT) {
            if (!_setR.add(key)) {
                _lralnv.writersUnlock();
                return false;
            }
            _lralnv.toggleVersionAndWait();
            _setL.add(key);
        } else {
            if (!_setL.add(key)) {
                _lralnv.writersUnlock();
                return false;
            }
            _lralnv.toggleVersionAndWait();
            _setR.add(key);
        }
        _lralnv.writersUnlock();
        return true;
    }


    /**
     * Checks if a key is in the set.
     *
     * Progress Condition: Wait-Free Population Oblivious (on x86)
     *
     * @param key
     * @return Returns true if the key is present in the set and false otherwise
     */
    bool contains(T key) {
    	int localLeftRight = _lralnv.arrive();
    	bool retValue;
        if (localLeftRight == LeftRight::READS_ON_LEFT) {
            retValue = _setL.contains(key);
        } else {
            retValue = _setR.contains(key);
        }
        _lralnv.depart(localLeftRight);
        return retValue;
    }


    /**
     * Removes a key from the set.
     *
     * Progress Condition: Blocking
     *
     * @param key
     * @return True if the key was successfully removed and false if there was
     *         no matching key in the set.
     */
    bool remove(T key) {
        _lralnv.writersLock();
        // Insert the node immediately on the unused list
        if (_lralnv.currentLeftRight() == LeftRight::READS_ON_LEFT) {
            if (!_setR.remove(key)) {
                _lralnv.writersUnlock();
                return false;
            }
            _lralnv.toggleVersionAndWait();
            _setL.remove(key);
        } else {
            if (!_setL.remove(key)) {
                _lralnv.writersUnlock();
                return false;
            }
            _lralnv.toggleVersionAndWait();
            _setR.remove(key);
        }
        _lralnv.writersUnlock();
        return true;
    }
};

#endif /* _LEFT_RIGHT_ATOMIC_LONG_NO_VERSION_ORDERED_LINKED_LIST_H_ */
