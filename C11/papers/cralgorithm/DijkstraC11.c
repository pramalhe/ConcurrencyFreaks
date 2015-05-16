// Edsger W. Dijkstra, Solution of a Problem in Concurrent Programming Control, CACM, 8(9), 1965, p. 569
#include <stdatomic.h>

#define CACHE_LINE  64
#define PADRATIO    (CACHE_LINE/sizeof(atomic_int))

static atomic_int *b CALIGN, *c CALIGN, turn CALIGN;

static void *Worker( void *arg ) {
	TYPE id = (size_t)arg + 1;							// id 0 => don't-want-in
	uint64_t entry;
#ifdef FAST
	unsigned int cnt = 0, oid = id;
#endif // FAST

	for ( int r = 0; r < RUNS; r += 1 ) {
		entry = 0;
		while ( atomic_load(&stop) == 0 ) {
			atomic_store(&b[id*PADRATIO], 0);									// entry protocol
		  L: atomic_store(&c[id*PADRATIO], 1);
			if ( atomic_load(&turn) != id ) {							// maybe set and restarted
				while ( atomic_load(&b[atomic_load(&turn)*PADRATIO]) != 1 ) Pause();			// busy wait
				atomic_store(&turn, id);
			} // if
			atomic_store(&c[id*PADRATIO], 0);
			for ( int j = 1; j <= N; j += 1 )
				if ( j != id && atomic_load(&c[j*PADRATIO]) == 0 ) goto L;
			CriticalSection( id );
			atomic_store(&b[id*PADRATIO], 1);
			atomic_store(&c[id*PADRATIO], 1);							// exit protocol
			atomic_store(&turn, 0);
#ifdef FAST
			id = startpoint( cnt );						// different starting point each experiment
			cnt = cycleUp( cnt, NoStartPoints );
#endif // FAST
			entry += 1;
		} // while
#ifdef FAST
		id = oid;
#endif // FAST
		entries[r][id - 1] = entry;						// adjust for id + 1
        atomic_fetch_add( &Arrived, 1 );
        while ( atomic_load(&stop) != 0 ) Pause();
        atomic_fetch_add( &Arrived, -1 );
	} // for
	return NULL;
} // Worker

void ctor() {
	b = Allocator( sizeof(__typeof__(b[0])) * (N + 1) * PADRATIO);
	c = Allocator( sizeof(__typeof__(c[0])) * (N + 1) * PADRATIO);
	for ( int i = 0; i <= N; i += 1 ) {					// initialize shared data
		c[i*PADRATIO] = ATOMIC_VAR_INIT(1);
		b[i*PADRATIO] = ATOMIC_VAR_INIT(1);
	} // for
	turn = ATOMIC_VAR_INIT(0);
} // ctor

void dtor() {
	free( (void *)c );
	free( (void *)b );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc -Wall -std=gnu99 -O3 -DNDEBUG -fno-reorder-functions -DPIN -DAlgorithm=Dijkstra Harness.c -lpthread -lm" //
// End: //
