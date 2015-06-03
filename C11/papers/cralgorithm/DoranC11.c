/*
 *
 *
 * WARNING: This only works for two threads !!!

 */

enum Intent { DontWantIn, WantIn };

#define CACHE_LINE  64
#define PADRATIO    (CACHE_LINE/sizeof(atomic_int))
#define inv( c ) ((c) ^ 1)
#define await( E ) while ( ! (E) ) Pause()

static atomic_int qarray[3*PADRATIO];
static atomic_int *intents[2] = { &qarray[0], &qarray[PADRATIO] };
static atomic_int *last = &qarray[2*PADRATIO];

static void *Worker( void *arg ) {
	TYPE id = (size_t)arg;
	uint64_t entry;
	int other = inv( id );
#ifdef FAST
	unsigned int cnt = 0, oid = id;
#endif // FAST

	for ( int r = 0; r < RUNS; r += 1 ) {
		entry = 0;
		while ( atomic_load(&stop) == 0 ) {
		    if ( id == 0 ) {
	            atomic_store(intents[id], WantIn);                       // declare intent
	            if ( atomic_load(intents[other]) == WantIn ) { // other thread want in ?
	                if ( atomic_load_explicit(last, memory_order_acquire) == id ) {                     // low priority task ?
	                    atomic_store(intents[id], DontWantIn);           // retract intent
	                    await( atomic_load(last) != id );                // low priority busy wait
	                    atomic_store(intents[id], WantIn);               // re-declare intent
	                } // if
	                await( atomic_load(intents[other]) == DontWantIn );  // high priority busy wait
	            } // if
		        CriticalSection( id );                      // critical section
	            atomic_store_explicit(last, id, memory_order_release);                                  // exit protocol
	            atomic_store_explicit(intents[id], DontWantIn, memory_order_release);
		    }
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
    assert( N == 2 );
    *intents[0] = ATOMIC_VAR_INIT(DontWantIn);
    *intents[1] = ATOMIC_VAR_INIT(DontWantIn);
    *last = ATOMIC_VAR_INIT(0);
} // ctor

void dtor() {
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc -Wall -std=gnu99 -O3 -DNDEBUG -fno-reorder-functions -DPIN -DAlgorithm=CorreiaRamalhete Harness.c -lpthread -lm" //
// End: //
