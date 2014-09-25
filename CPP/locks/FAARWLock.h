#ifndef __FAA_RWLOCK_H__
#define __FAA_RWLOCK_H__

#include <atomic>

// Use 0 for writer's "unlocked" and 1 for "locked" state
#define FAA_RWL_UNLOCKED    0
#define FAA_RWL_LOCKED      1


/* This is not recursive/reentrant */
class FAARWLock {
public:
	FAARWLock();
	~FAARWLock();
	void sharedLock(void);
	bool sharedUnlock(void);
	void exclusiveLock(void);
	bool exclusiveUnlock(void);

private:
    std::atomic<int>   readers_count;    /* Number of Readers */
    std::atomic<int>   writers_mutex;    /* lock/unlocked in write-mode */
};


#endif
