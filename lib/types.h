/**
Types definitions
*/

#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef float float32_t;
typedef double float64_t;
typedef long double float128_t;

#define safe __attribute__((transaction_safe))

#endif
