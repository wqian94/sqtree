/**
Point
*/

#ifndef POINT_H
#define POINT_H

#include <stdio.h>

#include "types.h"

#define abs(x) ((1 - 2 * ((x) < 0)) * (x))
#define PRECISION 1e-6

#ifdef DIMENSIONS
#define D DIMENSIONS
#endif

extern __thread rlu_thread_data_t *rlu_self;

/**
 * struct Point_t
 *
 * Represents D-dimensional data. Contains D members.
 */
typedef struct Point_t{
    float64_t data[D];
} Point;

/**
 * Point_init
 *
 * Returns a Point that represents the D-dimensional point given by the D
 * arguments provided.
 *
 * data... - the variadic list of arguments, should be D elements.
 *
 * Produces the representative Point.
 */
#define Point_init(data...) ((Point){data})

/**
 * Point_from_array
 *
 * Returns a Point that represents (data[0], data[1], ...).
 *
 * data - the coordinates of the point
 *
 * Returns a Point representing (data[0], data[1], ...).
 */
Point Point_from_array(float64_t data[D]);

/**
 * Point_compare
 *
 * Returns a value indicating whether a is <, =, or > b.
 *
 * A positive value indicates that b < a, 0 indicates equivalence, and a negative
 * value indicates that b > a. The x coordinates are compared first, and if they
 * are within the precision range, than the y coordinates are compared.
 *
 * a - the point to compare against
 * b - the point to compare
 *
 * Returns a value <0 if a < b, =0 if a == b, and >0 if a > b.
 */
int8_t Point_compare(Point *a, Point *b);

/**
 * Point_equals
 *
 * Returns true if the two points are within precision error of each other in both
 * coordinates.
 *
 * a - the first point to compare
 * b - the second point to compare
 *
 * Returns true if the two points are equal, up to precision error, and false otherwise.
 */
safe bool Point_equals(Point *a, Point *b);

/**
 * Point_copy
 *
 * Copies the coordinate data from the first point to the second.
 *
 * from - the point to copy from
 * to - the point to copy to
 */
safe void Point_copy(Point *from, Point *to);

static void Point_string(Point *p, char *buffer) {
    sprintf(buffer, "Point(%lf", p->data[0]);
    register uint64_t i;
    for (i = 1; i < D; i++) {
        sprintf(buffer, "%s, %lf", buffer, p->data[i]);
    }
    sprintf(buffer, "%s)", buffer);
}

#endif
