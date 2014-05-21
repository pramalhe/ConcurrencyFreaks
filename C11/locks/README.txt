This folder contains multiple implementations of Mutual Exclusion Locks in C11.
They are cross-platform.


-------------------------------------------------------------------------------
Ticket Lock      

ticket_mutex.h 
ticket_mutex.c
A simple Ticket Lock based on the algorithm described here:
http://web.mit.edu/6.173/www/currentsemester/readings/R06-scalable-synchronization-1991.pdf
with the optimizations described in this post:
http://concurrencyfreaks.com/2014/05/relaxed-atomics-optimizations-for.html



-------------------------------------------------------------------------------
Multi-Producer-Single-Consumer Mutex

mpsc_mutex.h
mpsc_mutex.c
A mutual exclusion lock that uses the MPSC queue invented by Dmitry Vyukov.
More details can be seen here:
http://concurrencyfreaks.com/2014/05/c11-atomics-and-mpsc-mutual-exclusion.html
and a powerpointe presentation here:
https://github.com/pramalhe/ConcurrencyFreaks/tree/master/presentations/MPSC-Mutex.pptx



-------------------------------------------------------------------------------
EXCHange Mutex

exchg_mutex.h
exchg_mutex.c
A mutual exclusion lock that combines ideas from the well known MSC lock and
from the MPSC queue. More details can be seen here:
???




