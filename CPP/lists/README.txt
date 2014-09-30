C++14 implementations of several linked-list based set.
These are easy to transform to maps if needed.

LinkedListSet.h - A single-thread only linked list set. The Left-Right pattern uses two instances of this to create a set with WFPO read-only operations
LROrderedLinkedList.h - LROLL: a LinkedListSet protected with the Left-Right pattern using DCLC readIndicator
LROrderedLinkedListSingle.h - LROLLS: Less memory usage than LROLL and 2x faster insertions and removals
RWLockLinkedListDCLC.h - Uses a DCLCRWLock (based on C-RW-WP) to provide concurrent (blocking) access to a LinkedListSet
RWLockLinkedListPT.h - Uses a pthread_rwlock_t to provide concurrent (blocking) access to a LinkedListSet
RWLockLinkedListSM.h - Uses a std::shared_time_mutex to provide concurrent (blocking) access to a LinkedListSet

