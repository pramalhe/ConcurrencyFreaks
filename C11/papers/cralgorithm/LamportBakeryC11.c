// Leslie Lamport, A New Solution of Dijkstra's Concurrent Programming Problem, CACM, 1974, 17(8), p. 454
#include <stdatomic.h>
atomic_int *choosing CALIGN, *ticket CALIGN;			// no "static" on SPARC

static void *Worker( void *arg ) {
	TYPE id = (size_t)arg;
	uint64_t entry;
#ifdef FAST
	unsigned int cnt = 0, oid = id;
#endif // FAST

	for ( int r = 0; r < RUNS; r += 1 ) {
		entry = 0;
		while ( atomic_load(&stop) == 0 ) {
			// step 1, select a ticket
			atomic_store(&choosing[id], 1);							// entry protocol
			int max = 0;								// O(N) search for largest ticket
			for ( int j = 0; j < N; j += 1 ) {
				int v = atomic_load(&ticket[j]);						// could change so must copy
				if ( max < v ) max = v;
			} // for
			max += 1;									// advance ticket
			atomic_store(&ticket[id], max);
			atomic_store(&choosing[id], 0);
			// step 2, wait for ticket to be selected
			for ( int j = 0; j < N; j += 1 ) {			// check other tickets
				while ( atomic_load(&choosing[j]) == 1 ) Pause();		// busy wait if thread selecting ticket
				while ( atomic_load(&ticket[j]) != 0 &&				// busy wait if choosing or
						( atomic_load(&ticket[j]) < max ||			//  greater ticket value or lower priority
						( atomic_load(&ticket[j]) == max && j < id ) ) ) Pause();
			} // for
			CriticalSection( id );
			atomic_store(&ticket[id], 0);								// exit protocol
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
	choosing = Allocator( sizeof(__typeof__(choosing[0])) * N );
	ticket = Allocator( sizeof(__typeof__(ticket[0])) * N );
	for ( int i = 0; i < N; i += 1 ) {					// initialize shared data
		choosing[i] = ATOMIC_VAR_INIT(0);
		ticket[i] = ATOMIC_VAR_INIT(0);
	} // for
} // ctor

void dtor() {
	free( (void *)ticket );
	free( (void *)choosing );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc-4.9 -std=c11 -Wall -O3 -DNDEBUG -DPIN -DAlgorithm=LamportBakeryC11 Harness.c -lpthread -lm" //
// End: //
