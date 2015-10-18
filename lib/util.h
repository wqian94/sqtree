/**
Some utility functions
*/

#ifndef UTIL_H
#define UTIL_H

#include <pthread.h>
#include <stdint.h>

#include "types.h"

/*******************************
** Marsaglia RNG
*******************************/

/**
 * MarsagliaXORV
 *
 * Provided by @amatveev
 */
uint32_t MarsagliaXORV(uint32_t x);

/**
 * MarsagliaXOR
 *
 * Provided by @amatveev
 */
uint32_t MarsagliaXOR(uint32_t* seed);

safe uint32_t Marsaglia_rand();

/**
 * Marsaglia_random
 *
 * Returns a random value between 0 (inclusive) and 1 (exclusive).
 */
safe double Marsaglia_random();

/**
 * Marsaliga_rands
 *
 * Generates a pseudorandom integer from the given seed. The seed is mutated in-place.
 *
 * seed - the seed to use
 *
 * Returns a random integer.
 */
uint32_t Marsaglia_rands(uint32_t *seed);

/**
 * Marsaliga_randoms
 *
 * Generates a pseudorandom value from the given seed. The seed is mutated in-place.
 *
 * seed - the seed to use
 *
 * Returns a random value between [0, 1).
 */
double Marsaglia_randoms(uint32_t *seed);

void Marsaglia_srand(uint32_t nseed);

/*******************************
** Parallel Marsaglia RNG
*******************************/

/**
 * Marsaglia_parallel_start
 *
 * Starts an array of seeds for the RNG, one for each thread.
 *
 * nthreads - the number of seeds to generate
 */
void Marsaglia_parallel_start(const uint64_t nthreads);

/**
 * Marsaglia_parallel_end
 *
 * Deallocates resources used for parallelized Marsaglia.
 */
void Marsaglia_parallel_end();

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
uint32_t* Marsaglia_parallel_init(const uint64_t vid);

/**
 * Marsaglia_parallel_get
 *
 * Returns the address for the seed for the current thread, or NULL if there is no
 * associated seed. This is a somewhat expensive operation, and should be used sparingly.
 */
uint32_t* Marsaglia_parallel_get();

/*******************************
** pthread mutex attr
*******************************/

void pthread_mutex_attr_init();

pthread_mutexattr_t* pthread_mutex_attr();

void pthread_mutex_attr_destroy();

#endif
