// James E. Burns and Nancy A. Lynch, {Mutual Exclusion using Indivisible Reads and Writes, Proceedings of the 18th
// Annual Allerton Conference on Communications, Control and Computing, 1980, p. 836
#include <stdatomic.h>

#define CACHE_LINE  64
#define PADRATIO    (CACHE_LINE/sizeof(atomic_int))

enum Intent { DontWantIn, WantIn };

// TODO: add padding
static atomic_int *intents CALIGN;					// shared

static void *Worker( void *arg ) {
    TYPE id = (size_t)arg;
	uint64_t entry;
#ifdef FAST
	unsigned int cnt = 0, oid = id;
#endif // FAST

	for ( int r = 0; r < RUNS; r += 1 ) {
		entry = 0;
		while ( atomic_load(&stop) == 0 ) {
		  L0: atomic_store(&intents[id*PADRATIO], DontWantIn);					// entry protocol
			for ( int j = 0; j < id; j += 1 )
				if ( atomic_load(&intents[j*PADRATIO]) == WantIn ) { Pause(); goto L0; }
			atomic_store(&intents[id*PADRATIO], WantIn);
			for ( int j = 0; j < id; j += 1 )
				if ( atomic_load(&intents[j*PADRATIO]) == WantIn ) goto L0;
		  L1: for ( int j = id + 1; j < N; j += 1 )
				if ( atomic_load(&intents[j*PADRATIO]) == WantIn ) { Pause(); goto L1; }
			CriticalSection( id );						// critical section
			atomic_store(&intents[id*PADRATIO], DontWantIn);					// exit protocol
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
	intents = Allocator( sizeof(__typeof__(intents[0])) * N * PADRATIO);
	for ( int i = 0; i < N; i += 1 ) {					// initialize shared data
		intents[i*PADRATIO] = ATOMIC_VAR_INIT(DontWantIn);
	} // for
} // ctor

void dtor() {
	free( (void *)intents );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc -Wall -std=gnu99 -O3 -DNDEBUG -fno-reorder-functions -DPIN -DAlgorithm=BurnsLynchRetract Harness.c -lpthread -lm" //
// End: //
