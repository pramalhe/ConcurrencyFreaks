Classes in the com.concurrencyfreaks.counter package:

DistributedCacheLineCounter - A Distributed Cache Line Counter (DCLC) that 
    scales well for calls to increment() and decrement(). It is wait-free (WFPO) 
    for increment() and decrement() as long as getAndAdd() is wait-free (which 
    depends on the JVM version and the CPU).
    http://concurrencyfreaks.com/2013/08/concurrency-pattern-distributed-cache.html
    
DistributedCacheLineCounterRelax - Similar to DCLC but uses relaxed optimization
    described in this post:
    http://concurrencyfreaks.com/2014/12/relaxed-atomics-on-array-of-counters.html    
    
OffByXCounter - A Counter that uses the "Off-by-X" pattern.
    http://concurrencyfreaks.com/2013/08/concurrency-pattern-off-by-x-counter.html
        
ResLinMonStatisticalCounter - A statistical counter similar to DCLC but it has a 
    reset() method which is linearizable with all other methods in the class.
    http://...        