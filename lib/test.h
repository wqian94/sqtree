/**
Testing suite utilities header
*/

#ifndef QUADTREE_TEST_H
#define QUADTREE_TEST_H

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <assert.h>
#include <mcheck.h>

#include "types.h"
#include "assertions.h"
#include "util.h"
#include "Quadtree.h"

extern uint32_t Marsaglia_rand();
#define rand() Marsaglia_rand()

typedef struct {
    bool on;
    uint32_t *food;
    uint32_t *first;
    uint32_t *last;
} TestRandTrough;

static TestRandTrough test_rand_trough = {.on = false, .food = NULL, .first = NULL, .last = NULL};

void test_rand_feed(uint32_t *food, uint32_t length) {
    test_rand_trough.on = true;
    test_rand_trough.first = food;
    test_rand_trough.last = food + length - 1;
    test_rand_trough.food = test_rand_trough.first;
}

void test_rand_on() {
    test_rand_trough.on = true;
}

void test_rand_off() {
    test_rand_trough.on = false;
}

void test_rand_close() {
    test_rand_trough.on = false;
    test_rand_trough.first = NULL;
    test_rand_trough.last = NULL;
    test_rand_trough.food = NULL;
}

int test_rand() {
    int next = rand();
    if (test_rand_trough.on) {
        next = *test_rand_trough.food;
        if (test_rand_trough.food == test_rand_trough.last)
            test_rand_trough.food = test_rand_trough.first;
        else
            test_rand_trough.food++;
    }
    return next;
}

static uint64_t TOTAL_TESTS = 0, PASSED_TESTS = 0;

void start_test(void (*func)(), const char *suite_name) {
    uint64_t prev_total_asserts = total_assertions();
    uint64_t prev_passed_asserts = passed_assertions();
    printf("\n===Testing %s===\n", suite_name);
    func();
    printf("\n");
    test_rand_close();
    TOTAL_TESTS++;
    PASSED_TESTS += (passed_assertions() - prev_passed_asserts) == (total_assertions() - prev_total_asserts);
}

long long total_tests() {
    return TOTAL_TESTS;
}

long long passed_tests() {
    return PASSED_TESTS;
}

#endif
