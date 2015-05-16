// Eric C. R. Hehner and R. K. Shyamasundar, An Implementation of P and V, Information Processing Letters, 1981, 12(4),
// pp. 196-197
#include <stdatomic.h>
enum { MAX_TICKET = INTPTR_MAX };

static atomic_llong *ticket CALIGN;

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
		    atomic_store(&ticket[id], 0);								// set highest priority
		    uint64_t max = 0;								// O(N) search for largest ticket
			for ( int j = 0; j < N; j += 1 ) {
			    uint64_t v = atomic_load(&ticket[j]);						// could change so copy
				if ( max < v && v != MAX_TICKET ) max = v;
			} // for
			max += 1;									// advance ticket
			atomic_store(&ticket[id], max);
			// step 2, wait for ticket to be selected
			for ( int j = 0; j < N; j += 1 )			// check other tickets
				while ( atomic_load(&ticket[j]) < max ||				// busy wait if choosing or
						( atomic_load(&ticket[j]) == max && j < id ) ) Pause(); //  greater ticket value or lower priority
			CriticalSection( id );
			atomic_store(&ticket[id], MAX_TICKET);					// exit protocol
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
	ticket = Allocator( sizeof(__typeof__(ticket[0])) * N );
	for ( int i = 0; i < N; i += 1 ) {					// initialize shared data
		ticket[i] = ATOMIC_VAR_INIT(MAX_TICKET);
	} // for
} // ctor

void dtor() {
	free( (void *)ticket );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc-4.9 -std=c11 -Wall -O3 -DNDEBUG -DPIN -DAlgorithm=HehnerC11 Harness.c -lpthread -lm" //
// End: //
