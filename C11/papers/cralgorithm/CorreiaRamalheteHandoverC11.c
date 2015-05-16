// Correia and Ramalhete CRHandover, Mutual Exclusion - Two linear wait software solutions
// https://github.com/pramalhe/ConcurrencyFreaks/tree/master/papers/cralgorithm-2015.pdf
//
// Shared words      = 2N+1
// Number of states  = 3 + 2
// Starvation-Free   = yes, with N
// Minimum SC stores = 1 + 1
// Minimum SC loads  = 2 + 2
//
#include <stdatomic.h>

enum State { UNLOCKED, WAITING, LOCKED };
enum Handover { NOTMINE, MYTURN };

#define CACHE_LINE  64
#define PADRATIO    (CACHE_LINE/sizeof(atomic_int))

static atomic_int *states CALIGN;              // shared
static atomic_int *handover CALIGN;            // shared
static atomic_int  hoEnabled CALIGN;           // shared

inline static int validate_left(int id) {
    for (int i = 0; i < id; i++) {
        if (atomic_load(&states[i]) != UNLOCKED) return 0;
    }
    return 1;
}

inline static int validate_right(int id) {
    for (int i = id + 1; i < N; i++) {
        if (atomic_load(&states[i]) == LOCKED) return 0;
    }
    return 1;
}

static void *Worker( void *arg ) {
    TYPE id = (size_t)arg;
    uint64_t entry;
#ifdef FAST
    unsigned int cnt = 0, oid = id;
#endif // FAST

    for ( int r = 0; r < RUNS; r += 1 ) {
        entry = 0;
        while ( atomic_load(&stop) == 0 ) {
            int i, isFirstTime = 1;
            while (1) {
                while (atomic_load(&hoEnabled)) {
                    if (isFirstTime) { atomic_store(&states[id*PADRATIO], WAITING); isFirstTime = 0; }
                    if (atomic_load(&handover[id*PADRATIO]) == MYTURN) {
                        atomic_store_explicit(&handover[id*PADRATIO], NOTMINE, memory_order_relaxed);
                        atomic_store_explicit(&states[id*PADRATIO], LOCKED, memory_order_relaxed);
                        goto LCS; // fast-path under high contention
                    }
                    Pause();
                }
                isFirstTime = 0;
                atomic_store(&states[id*PADRATIO], LOCKED);
                for (i = 0; i < id; i++) {
                    if (atomic_load(&states[i*PADRATIO]) != UNLOCKED) break;
                }
                if (i != id) {
                    atomic_store(&states[id*PADRATIO], WAITING);
                    while (!atomic_load(&hoEnabled)) {
                        for (i = 0; i < id; i++) {
                            if (atomic_load(&states[i*PADRATIO]) != UNLOCKED) break;
                        }
                        if (i == id) break;
                        Pause();
                    }
                    continue;
                }
                while (!atomic_load(&hoEnabled)) {
                    for (i = id + 1; i < N; i++) {
                        if (atomic_load(&states[i*PADRATIO]) == LOCKED) break;
                    }
                    if (i == N && !atomic_load(&hoEnabled)) goto LCS;
                }
                atomic_store(&states[id*PADRATIO], WAITING);
            }
          LCS: CriticalSection( id );                      // critical section
            for (int i = id + 1; i < N; i++) {
                if (atomic_load(&states[i*PADRATIO]) == WAITING) {
                    if (!atomic_load_explicit(&hoEnabled, memory_order_relaxed)) atomic_store(&hoEnabled, 1);
                    atomic_store(&handover[i*PADRATIO], MYTURN);
                    atomic_store(&states[id*PADRATIO], UNLOCKED);
                    goto LEND;
                }
            }
            for (int i = 0; i < id; i++) {
                if (atomic_load(&states[i*PADRATIO]) == WAITING) {
                    if (!atomic_load_explicit(&hoEnabled, memory_order_relaxed)) atomic_store(&hoEnabled, 1);
                    atomic_store(&handover[i*PADRATIO], MYTURN);
                    atomic_store(&states[id*PADRATIO], UNLOCKED);
                    goto LEND;
                }
            }
            // There are no successors at all
            if (atomic_load_explicit(&hoEnabled, memory_order_relaxed)) atomic_store(&hoEnabled, 0);
            atomic_store(&states[id*PADRATIO], UNLOCKED);
          LEND:
#ifdef FAST
            id = startpoint( cnt );                     // different starting point each experiment
            cnt = cycleUp( cnt, NoStartPoints );
#endif // FAST
            entry += 1;
        } // while
#ifdef FAST
        id = oid;
#endif // FAST
        entries[r][id] = entry;
        atomic_fetch_add( &Arrived, 1 );
        while ( atomic_load(&stop) != 0 ) Pause();
        atomic_fetch_add( &Arrived, -1 );
    } // for
    return NULL;
} // Worker

void ctor() {
    states = Allocator( sizeof(__typeof__(states[0])) * N * PADRATIO );
    handover = Allocator( sizeof(__typeof__(handover[0])) * N * PADRATIO );
    for ( int i = 0; i < N; i += 1 ) {                  // initialize shared data
        states[i*PADRATIO] = ATOMIC_VAR_INIT(UNLOCKED);
        handover[i*PADRATIO] = ATOMIC_VAR_INIT(NOTMINE);
    } // for
    hoEnabled = ATOMIC_VAR_INIT(0); // false
} // ctor

void dtor() {
    free( (void *)states );
    free( (void *)handover );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc-4.9 -std=c11 -Wall -O3 -DNDEBUG -DAlgorithm=CorreiaRamalheteHandoverC11 HarnessC11.c -lpthread -lm" //
// End: //
