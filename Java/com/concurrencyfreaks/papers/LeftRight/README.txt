Classes used in the benchmark shown in the article 
"Left-Right: A Concurrency Control Technique with Wait-Free Population Oblivious Reads"

RWLockTreeSet.java: java.util.TreeSet protected with a Reader-Writer lock 
    ScalableRWLock. The ScalableRWLock is a freely available 
    lock that uses the C-RW-WP lock described in the paper "NUMA Aware Reader-Writer Locks"
    with a readIndicator we have created that combines an array and ConcurrentLinkedQueue.
    We also use Java's StampedLock but only as a mutual exclusion lock for Writers.
    The add() and remove() are protected with exclusiveLock(), and the contains() 
    with the sharedLock() and, therefore, all operations are blocking.
    
BenchmarkTreeSetLatency.java: Measures latency on the following classes.

BenchmarkTreeSetFullRebalance.java: Measures performance on the following classes.
    
LRScalableTreeSet.java: java.util.TreeSet with the classic Left-Right technique 
    described in the Left-Right paper.
    
LRScalableTreeSetNV.java: java.util.TreeSet with the Left-Right technique 
    (No Version) without using a versionIndex, as described in the Left-Right paper.
    
LRScalableTreeSetRV.java: java.util.TreeSet with the Left-Right technique 
   (Reader's Version) where each Reader updates its own version that replaces the 
   state, as described in the Left-Right paper.
    
LRScalableTreeSetOptimistic.java: java.util.TreeSet with the optimistic 
    approach as described in the Left-Right paper.
    


