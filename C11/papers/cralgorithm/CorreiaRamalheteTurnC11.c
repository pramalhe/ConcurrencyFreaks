// Correia and Ramalhete CRTurn, Mutual Exclusion - Two linear wait software solutions
// https://github.com/pramalhe/ConcurrencyFreaks/tree/master/papers/cralgorithm-2015.pdf
//
//
// Shared words      = N+1
// Number of states  = 3
// Starvation-Free   = yes, with N
// Minimum SC stores = 1 + 1
// Minimum SC loads  = N+2

#include <stdatomic.h>

enum State { UNLOCKED, WAITING, LOCKED };

#define CACHE_LINE  64
#define PADRATIO    (CACHE_LINE/sizeof(atomic_int))

static atomic_int *states CALIGN;					// shared
static atomic_int  turn CALIGN;                     // shared

inline static int validate_left(int id, int lturn) {
    int i;
    if (lturn > id) {
        for (i = lturn; i < N; i++) {
            if (atomic_load(&states[i*PADRATIO]) != UNLOCKED) return 0;
        }
        for (i = 0; i < id; i++) {
            if (atomic_load(&states[i*PADRATIO]) != UNLOCKED) return 0;
        }
    } else {
        for (i = lturn; i < id; i++) {
            if (atomic_load(&states[i*PADRATIO]) != UNLOCKED) return 0;
        }
    }
    return 1;
}

inline static int validate_right(int id, int lturn) {
    int i;
    if (lturn <= id) {
        for (i = id + 1; i < N; i++) {
            if (atomic_load(&states[i*PADRATIO]) == LOCKED) return 0;
        }
        for (i = 0; i < lturn; i++) {
            if (atomic_load(&states[i*PADRATIO]) == LOCKED) return 0;
        }
    } else {
        for (i = id + 1; i < lturn; i++) {
            if (atomic_load(&states[i*PADRATIO]) == LOCKED) return 0;
        }
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
            atomic_store(&states[id*PADRATIO], LOCKED);
            while (1) {
                int lturn = atomic_load(&turn);
                if (!validate_left(id, lturn)) {
                    atomic_store(&states[id*PADRATIO], WAITING);
                    while (1) {
                        if (validate_left(id, lturn) && lturn == atomic_load(&turn)) break;
                        Pause();
                        lturn = atomic_load(&turn);
                    }
                    atomic_store(&states[id*PADRATIO], LOCKED);
                    continue;
                }
                while (lturn == atomic_load(&turn)) {
                    if (validate_right(id, lturn)) break;
                    Pause();
                }
                if (lturn == atomic_load(&turn)) break;
            }
			CriticalSection( id );						// critical section
			int lturn = (atomic_load_explicit(&turn, memory_order_relaxed)+1) % N;
			atomic_store_explicit(&turn, lturn, memory_order_relaxed);
			atomic_store(&states[id*PADRATIO], UNLOCKED);					// exit protocol
#ifdef FAST
			id = startpoint( cnt );						// different starting point each experiment
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
	states = Allocator( sizeof(__typeof__(states[0])) * N * PADRATIO);
	for ( int i = 0; i < N; i += 1 ) {					// initialize shared data
		states[i*PADRATIO] = ATOMIC_VAR_INIT(UNLOCKED);
	} // for
	turn = ATOMIC_VAR_INIT(0);
} // ctor

void dtor() {
	free( (void *)states );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc-4.9 -std=c11 -Wall -O3 -DNDEBUG -DPIN -DAlgorithm=CorreiaRamalheteSFC11 Harness.c -lpthread -lm" //
// End: //
