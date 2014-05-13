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

/*
 * Uses the algorithm of the Ticket Lock described in section 2.2 by
 * John Mellor-Crummey and Michael Scott in 1991:
 * http://web.mit.edu/6.173/www/currentsemester/readings/R06-scalable-synchronization-1991.pdf
 *
 * Notice that the initial decision of whether to spin or enter the critical
 * section is reach in a wait-free way on x86 or other systems for which
 * atomic_fetch_add() is implemented with a single atomic instruction (XADD in
 * the x86 case).
 */
#include "ticket_mutex.h"



void ticket_mutex_init(ticket_mutex_t * self)
{
    atomic_store(&self->ingress, 0);
    atomic_store(&self->egress, 0);
}


void ticket_mutex_destroy(ticket_mutex_t * self)
{
    // kind of unnecessary, but oh well
    atomic_store(&self->ingress, 0);
    atomic_store(&self->egress, 0);
}


/*
 *
 */
void ticket_mutex_lock(ticket_mutex_t * self)
{
    long lingress = atomic_fetch_add(&self->ingress, 1);
    while (lingress != atomic_load(&self->egress)) {
        sched_yield();  // Replace this with thrd_yield() if you use <threads.h>
    }
    // This thread has acquired the lock on the mutex
}


/*
 *
 */
void ticket_mutex_unlock(ticket_mutex_t * self)
{
    atomic_fetch_add(&self->egress, 1);
}

