CLH-Mutex.pptx: Just a short presentation with animations on how the CLH mutual 
exclusion lock works, to act as an introduction to the C11 implementation.

CLH-RWLock.pptx: A presentation with animations explaning how to merge the C-RW-WP algorithm
(scalable reader-writer lock) with the CLH mutex.

ConcurrentLinkedQueueRelaxed.pptx: A presentation explaining how to modify Java's ConcurrentLinkedQueue
to have relaxed load traversal, which improves the performance on PowerPC.

DCLCRelax.pptx: A presentation explaining the relaxed loads optimization on the 
DistributedCacheLineCounterRelax and other similar ReadIndicators/counters.

DoubleInstanceLocking.pptx: A presentation on the "Double Instance Locking" primitive
that uses two RW-Locks and one Mutex to provide Lock-Free read-only access with two 
instances of the same resource.

Lock-Free And Wait-Free Memory Reclamation.pptx: A presentation about manual non-blocking 
memory reclamation techniques, describing with animations techniques like Atomic Reference
Counting, Hazard Pointers, Hazard Eras, Userspace RCU (GraceVersion).

MPSC-Lock.pptx: A presentation with animations explaining how another variant of the CLH lock
works, this time using the MPSC queue by Dmitry Vyukov.



