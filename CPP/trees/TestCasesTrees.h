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


#ifndef _TEST_CASES_TREES_H_
#define _TEST_CASES_TREES_H_


enum test_case_enum_t {
    TC_TREES_RWL_PT,           // Reader-Writer lock std::map protected with a pthread_rwlock_t
    TC_TREES_RWL_SM,           // Reader-Writer lock std::map protected with std::shared_timed_mutex
    TC_TREES_RWL_DCLC,         // Reader-Writer lock std::map protected with DCLC-RWLock (C-RW-WP)
    TC_TREES_LRCLASSIC_ATOMIC,
    TC_TREES_LRCLASSIC_DCLC,
    TC_TREES_LRCLASSIC_PER_THREAD,
    TC_TREES_LRCLASSIC_LAMBDA,
    TC_TREES_COWLOCK_LRC_ATOMIC, // Copy-On-Write with Left-Right Classic (RIAtomicCounter)
    TC_TREES_MAX
};


// This must match the enums for test_case_enum_t
static std::string test_case_names[] = {
    "RWLockPthreadMap                     ",
    "RWLockSharedMutexMap                 ",
    "CRWWPMap                             ",
    "LRClassicMap (RIAtomicCounter)       ",
    "LRClassicMap (RIDCLC)                ",
    "LRClassicMap (RIEntryPerThread)      ",
    "LRCLambda (RIAtomicCounter+std::map) ",
    "COWLockMap (LRC+RIAtomicCounter)     "
};

#endif /* _TEST_CASES_H_ */
