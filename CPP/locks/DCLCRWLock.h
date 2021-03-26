#ifndef __DCLC_RWLOCK_H__
#define __DCLC_RWLOCK_H__

#include <atomic>

/*
 * TODO: Add the blabla here
 *
 * Disadvantages:
 * - Can't use this lock on code that can fork()
 * - Shouldn't use this on signal handler code (or any kind of lock for that matter)
 *
 *
 */

// Use 0 for writer's "unlocked" and 1 for "locked" state
#define DCLC_RWL_UNLOCKED    0
#define DCLC_RWL_LOCKED      1

// Cache line optimization constants
#define DCLC_CACHE_LINE          64               // Size in bytes of a cache line
#define DCLC_CACHE_PADD          (DCLC_CACHE_LINE-sizeof(std::atomic<int>))
#define DCLC_NUMBER_OF_CORES     32
#define DCLC_HASH_RATIO           3
#define DCLC_COUNTERS_RATIO      (DCLC_HASH_RATIO*DCLC_CACHE_LINE/sizeof(int))


/* This is not recursive/reentrant */
class DCLCRWLock {
public:
    DCLCRWLock();
    DCLCRWLock(int num_cores);
    ~DCLCRWLock();
    void sharedLock(void);
    bool trySharedLock(void);
    bool sharedUnlock(void);
    void exclusiveLock(void);
    bool tryExclusiveLock(void);
    bool exclusiveUnlock(void);

private:
    int thread2idx(void);

private:
    /* Hash Function for thread id to integer */
    std::hash<std::thread::id> hashFunc;
    /* Number of cores on the system */
    int          numCores;
    /* Length of readers_counters[] */
    int          countersLength;
    /* Distributed Counters for Readers */
    std::atomic<int>  *readersCounters;
    /* Padding */
    char               pad1[DCLC_CACHE_PADD];
    /* lock/unlocked in write-mode */
    std::atomic<int>   writersMutex;
};


#endif
