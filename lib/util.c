/**
Some utility functions
*/

#include "./util.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*******************************
** Marsaglia RNG
*******************************/

/**
 * MarsagliaXORV
 *
 * Provided by @amatveev
 */
uint32_t MarsagliaXORV(uint32_t x) {
    if (x == 0)
        x = 1;
    x ^= x << 6;
    x ^= ((unsigned)x) >> 21;
    x ^= x << 7 ;
    return x;  // use either x or x & 0x7FFFFFFF
}

/**
 * MarsagliaXOR
 *
 * Provided by @amatveev
 */
uint32_t MarsagliaXOR(uint32_t* seed) {
    unsigned x = MarsagliaXORV(*seed);
    *seed = x;
    return x & 0x7FFFFFFF;
}

static uint32_t Marsaglia_seed;

uint32_t Marsaglia_rand() {
    return (unsigned)MarsagliaXOR(&Marsaglia_seed);
}

/**
 * Marsaglia_random
 *
 * Returns a random value between 0 (inclusive) and 1 (exclusive).
 */
double Marsaglia_random() {
    const uint32_t denom = ((uint32_t)~0) >> 1;
    const uint32_t num = Marsaglia_rand();
    return (num % denom) / (double)denom;
}

/**
 * Marsaliga_rands
 *
 * Generates a pseudorandom integer from the given seed. The seed is mutated in-place.
 *
 * seed - the seed to use
 *
 * Returns a random integer.
 */
uint32_t Marsaglia_rands(uint32_t *seed) {
    return (unsigned)MarsagliaXOR(seed);
}

/**
 * Marsaliga_randoms
 *
 * Generates a pseudorandom value from the given seed. The seed is mutated in-place.
 *
 * seed - the seed to use
 *
 * Returns a random value between [0, 1).
 */
double Marsaglia_randoms(uint32_t *seed) {
    const uint32_t denom = ((uint32_t)~0) >> 1;
    const uint32_t num = Marsaglia_rands(seed);
    return (num % denom) / (double)denom;
}

void Marsaglia_srand(uint32_t nseed) {
    Marsaglia_seed = nseed;
}

/*******************************
** Parallel Marsaglia RNG
*******************************/

static uint32_t *seeds = NULL;
static uint64_t *pthread_ids = NULL;
static uint64_t Marsaglia_nthreads;

/**
 * Marsaglia_parallel_start
 *
 * Starts an array of seeds for the RNG, one for each thread.
 *
 * nthreads - the number of seeds to generate
 */
void Marsaglia_parallel_start(const uint64_t nthreads) {
    seeds = (uint32_t*)malloc(sizeof(*seeds) * nthreads);
    pthread_ids = (uint64_t*)malloc(sizeof(*pthread_ids) * nthreads);
    register uint64_t i;
    for (i = 0; i < nthreads; i++)
        pthread_ids[i] = -1;  // highest possible thread ID?
    Marsaglia_nthreads = nthreads;
}

/**
 * Marsaglia_parallel_end
 *
 * Deallocates resources used for parallelized Marsaglia.
 */
void Marsaglia_parallel_end() {
    free(seeds);
    free(pthread_ids);
    seeds = NULL;
    pthread_ids = NULL;
}

/**
 * Marsaglia_parallel_init
 *
 * Initializes a Marsaglia parallel seed and associates it with the thread's
 * pthread_self() ID, given the virtual ID.
 *
 * vid - the thread's virtual ID [0, NTHREADS)
 *
 * Returns a pointer to the associated seed.
 */
uint32_t* Marsaglia_parallel_init(const uint64_t vid) {
    pthread_ids[vid] = (uint64_t)pthread_self();
    seeds[vid] = Marsaglia_seed + vid * vid;
    return seeds + vid;
}

/**
 * Marsaglia_parallel_get
 *
 * Returns the address for the seed for the current thread, or NULL if there is no
 * associated seed. This is a somewhat expensive operation, and should be used sparingly.
 * If the parallel infrastructure has not been started, this returns the global seed.
 */
uint32_t* Marsaglia_parallel_get() {
    if (seeds == NULL)
        return &Marsaglia_seed;

    uint64_t self = (uint64_t)pthread_self();
    register uint64_t i;
    for (i = 0; i < Marsaglia_nthreads; i++)
        if (pthread_ids[i] == self)
            return seeds + i;
    return NULL;
}

/*******************************
** pthread mutex attr
*******************************/

static volatile pthread_mutexattr_t lock_attr, *lock_attr_ptr = NULL;

void pthread_mutex_attr_init() {
    pthread_mutexattr_init((pthread_mutexattr_t*)&lock_attr);
    pthread_mutexattr_settype((pthread_mutexattr_t*)&lock_attr, PTHREAD_MUTEX_ERRORCHECK);
    lock_attr_ptr = &lock_attr;
}

pthread_mutexattr_t* pthread_mutex_attr() {
    return (pthread_mutexattr_t*)lock_attr_ptr;
}

void pthread_mutex_attr_destroy() {
    lock_attr_ptr = NULL;
    pthread_mutexattr_destroy((pthread_mutexattr_t*)&lock_attr);
}
