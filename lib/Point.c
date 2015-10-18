/**
Point implementation
*/

#include "Point.h"

Point Point_from_array(float64_t data[D]) {
    Point p;
    uint64_t i;
    for (i = 0; i < D; i++) {
        p.data[i] = data[i];
    }
    return p;
}

int8_t Point_compare(Point *a, Point *b) {
    register uint64_t i;
    for (i = 0; i < D; i++)
        if (abs(a->data[i] - b->data[i]) > PRECISION)
            return (2 * (a->data[i] > b->data[i]) - 1);
}

bool Point_equals(Point *a, Point *b) {
    register uint64_t i;
    for (i = 0; i < D; i++)
        if (abs(a->data[i] - b->data[i]) > PRECISION)
            return false;
    return true;
}

void Point_copy(Point* from, Point* to) {
    register uint64_t i;
    for (i = 0; i < D; i++)
        to->data[i] = from->data[i];
}
