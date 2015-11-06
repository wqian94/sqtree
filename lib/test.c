/**
Testing suite for correctness of Quadtrees
*/

#include "test.h"

extern bool in_range(Node*, Point*);
extern void Point_string(Point*, char*);

void print_Quadtree(Quadtree *root) {
    register uint64_t i;

    char buffer[1000];

    printf("Node[pointer=%p, ", root);
#ifdef QUADTREE_TEST
    printf("id=%llu, ", (unsigned long long)root->id);
#endif
    Point_string(&root->center, buffer);
    printf("center=%s, length=%lf, is_square=%d", 
        buffer + 5, root->length, root->is_square);

    if (root->parent != NULL
            #ifdef PARALLEL
            && !root->parent->dirty
            #endif
            )
#ifdef QUADTREE_TEST
        printf(", parent=%llu", (unsigned long long)root->parent->id);
#else
        printf(", parent=%p", root->parent);
#endif

    if (root->up != NULL
            #ifdef PARALLEL
            && !root->up->dirty
            #endif
            )
#ifdef QUADTREE_TEST
        printf(", up=%llu", (unsigned long long)root->up->id);
#else
        printf(", up=%p", root->up);
#endif

    if (root->down != NULL
            #ifdef PARALLEL
            && !root->down->dirty
            #endif
            )
#ifdef QUADTREE_TEST
        printf(", down=%llu", (unsigned long long)root->down->id);
#else
        printf(", down=%p", root->down);
#endif

    for (i = 0; i < (1LL << D); i++) {
        if (root->children[i] != NULL
                #ifdef PARALLEL
                && !root->children[i]->dirty
                #endif
                )
#ifdef QUADTREE_TEST
            printf(", children[%llu]=%llu", (unsigned long long)i, (unsigned long long)root->children[i]->id);
#else
            printf(", children[%llu]=%p", (unsigned long long)i, root->children[i]);
#endif
    }


    printf("]\n");

    if (root->up != NULL
            #ifdef PARALLEL
            && !root->up->dirty
            #endif
            )
        print_Quadtree(root->up);

    for (i = 0; i < (1LL << D); i++) {
        if (root->children[i] != NULL
                #ifdef PARALLEL
                && !root->children[i]->dirty
                #endif
                )
            print_Quadtree(root->children[i]);
    }
}

void test_sizes() {
    printf("dimensions        = %lu\n", (unsigned long)D);
    printf("sizeof(Quadtree)  = %lu\n", sizeof(Quadtree));
    printf("sizeof(bool)      = %lu\n", sizeof(bool));
    printf("sizeof(float64_t) = %lu\n", sizeof(float64_t));
    printf("sizeof(Node*)     = %lu\n", sizeof(Node*));
    printf("sizeof(Point)     = %lu\n", sizeof(Point));
    printf("\n===Testing Quadtree size===\n");
    // Quadtree is normally 40 bytes, but we add an id parameter for testing, so it is 48 bytes.
    // Then, each dimension adds 8 * 2 ^ D bytes for children, e.g. 2 dimensions -> 32 bytes.
    // Also, each dimension adds 8 * D bytes, e.g. 2 dimensions -> 16 bytes.
    #ifndef PARALLEL
    assertLong(48 + 8 * (1LL << D) + 8 * D, sizeof(Quadtree), "sizeof(Quadtree)");
    #endif
}

void test_in_range() {
    register uint64_t i;

    float64_t coords[D];
    for (i = 0; i < D; i++) coords[i] = 0;
    Quadtree *node = Quadtree_init(2.0, Point_from_array(coords));
    for (i = 0; i < D; i++) coords[i] = -1;
    Point p1 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 2;
    Point p2 = Point_from_array(coords);
    // Dynamically determine appropriate buffer sizes
    char buffer[1000 + 9 * (1LL << D)], point_buffer[15 * D], node_buffer[1000 + 15 * (1LL << D)];

    Node_string(node, node_buffer);

    Point_string(&p1, point_buffer);
    sprintf(buffer, "in_range(node, %s)", point_buffer);
    assertTrue(in_range(node, &p1), buffer);

    Point_string(&p2, point_buffer);
    sprintf(buffer, "in_range(node, %s)", point_buffer);
    assertFalse(in_range(node, &p2), buffer);

    Quadtree_free(node);
}

void test_get_quadrant() {
    register uint64_t i, j;

    float64_t coords[D];
    for (i = 0; i < D; i++) coords[i] = 0;
    Point origin = Point_from_array(coords);

    Point points[1LL << D];
    uint64_t expected_quadrants[1LL << D];
    for (i = 0; i < (1LL << D); i++) {
        for (j = 0; j < D; j++) {
            coords[j] = 2 * ((i >> j) & 1) - 1.0f;
        }
        points[i] = Point_from_array(coords);
        expected_quadrants[i] = i;
    }

    char buffer[1000], str1[1000], str2[1000];
    Point_string(&origin, str1);
    for (i = 0; i < (1LL << D); i++) {
        Point_string(points + i, str2);
        sprintf(buffer, "get_quadrant(%s, %s)", str1, str2);
        assertLong(expected_quadrants[i], (unsigned long long)get_quadrant(&origin, &points[i]), buffer);
    }
}

void test_get_new_center() {
    register uint64_t i, j;

    float64_t coords[D];

    float64_t s1 = 16.0;  // size1; chose to use S instead of L
    for (i = 0; i < D; i++) coords[i] = 0;
    Point p1 = Point_from_array(coords);
    Quadtree *q1 = Quadtree_init(s1, p1);
    
    Point new_center = get_new_center(q1, 0);
    for (i = 0; i < D; i++) coords[i] = -4;
    assertPoint(Point_from_array(coords), new_center, "new_center");
    Quadtree_free(q1);
}

void test_quadtree_create() {
    register uint64_t i;

    float64_t coords[D];

    printf("\n---Quadtree_init Quadtree Or Square Test---\n");
    float64_t s1 = 16.0;  // size1; chose to use S instead of L
    for (i = 0; i < D; i++) coords[i] = 0;
    Point p1 = Point_from_array(coords);
    Quadtree *q1 = Quadtree_init(s1, p1);
    assertDouble(s1, q1->length, "q1->length");
    assertPoint(p1, q1->center, "q1->center");
    assertTrue(q1->is_square, "q1->is_square");

    printf("\n---Quadtree_init Node Test---\n");
    Node *q2 = Node_init(s1, p1);
    assertDouble(s1, q2->length, "q2->length");
    assertPoint(p1, q2->center, "q2->center");
    assertFalse(q2->is_square, "q2->is_square");

    Quadtree_free(q1);
    Quadtree_free(q2);
}

void test_quadtree_add() {
    register uint64_t i;

    float64_t coords[D];
    char buffer[1000], str1[1000], str2[1000];

    float64_t s1 = 16.0;  // size1; chose to use S instead of L
    for (i = 0; i < D; i++) coords[i] = 0;
    Point p1 = Point_from_array(coords);
    Quadtree *q1 = Quadtree_init(s1, p1);

    // engineer our own "random" values
    uint32_t rand_food[32] = {0, 0, 0, 0, 99, 0, 0, 99, 0, 0, 99, 0, 0, 0, 0, 99, 0, 99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 99};
    test_rand_feed(rand_food, 32);

    for (i = 0; i < D; i++) coords[i] = 1;
    Point p2 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 7;
    Point p3 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 3;
    Point p4 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = -2;
    Point p5 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 0.5;
    Point p6 = Point_from_array(coords);

    printf("\n---Quadtree_add One Node Test---\n");
    assertTrue(Quadtree_add(q1, p2), "Quadtree_add(q1, p2)");
    Node *q2 = q1->children[get_quadrant(&q1->center, &p2)];

    int count_q1_levels = 0;
    Node *node;
    for (node = q1; node != NULL; node = node->up)
        count_q1_levels++;
    Point_string(&q1->center, buffer);
    sprintf(buffer, "Levels of Node%s", buffer);
    assertLong(5, count_q1_levels, buffer);

    int count_q2_levels = 0;
    for (node = q2; node != NULL; node = node->up)
        count_q2_levels++;
    Point_string(&q2->center, buffer);
    sprintf(buffer, "Levels of Node%s", buffer);
    assertLong(5, count_q2_levels, buffer);

    printf("\n---Quadtree_add Conflicting Node Test---\n");
    assertTrue(Quadtree_add(q1, p3), "Quadtree_add(q1, p3)");
    Node *square1 = q1->children[get_quadrant(&q1->center, &p2)];
    for (i = 0; i < D; i++) coords[i] = 4;
    assertPoint(Point_from_array(coords), square1->center, "square1->center");

    sprintf(buffer, "q1->children[%llu].is_square", (unsigned long long)get_quadrant(&q1->center, &p2));
    assertTrue(square1->is_square, buffer);

    sprintf(buffer, "(q1->children[%llu]->children[%llu] == NULL)",
        (unsigned long long)get_quadrant(&q1->center, &p2), (unsigned long long)get_quadrant(&square1->center, &p3));
    Node *q3 = square1->children[get_quadrant(&square1->center, &p3)];
    assertFalse(q3 == NULL, buffer);

    if (q3 != NULL) {
        assertPoint(p3, q3->center, "q3->center");

        Point_string(&square1->center, str1);
        Point_string(&q3->center, str2);
        sprintf(buffer, "get_quadrant(%s, %s)", str1, str2);
        uint64_t quadrant = 0;
        for (i = 0; i < D; i++)
            quadrant |= ((q3->center.data[i] >= square1->center.data[i] - PRECISION) & 1) << i;
        assertLong((unsigned long long)quadrant, (unsigned long long)get_quadrant(&square1->center, &q3->center), buffer);
    }
    else {  // alert to problems
        sprintf(buffer, "(q1->children[%llu]->children[%llu] is not NULL",
            (unsigned long long)get_quadrant(&q1->center, &p2), (unsigned long long)get_quadrant(&square1->center, &p3));
        assertError(buffer);
        sprintf(buffer, "(q1->children[%llu]->children[%llu]->center is not NULL",
            (unsigned long long)get_quadrant(&q1->center, &p2), (unsigned long long)get_quadrant(&square1->center, &p3));
        assertError(buffer);
    }

    sprintf(buffer, "(q1->children[%llu]->children[0] == NULL)", (unsigned long long)get_quadrant(&q1->center, &p2));
    assertFalse(square1->children[0] == NULL, buffer);

    Point_string(&square1->center, str1);
    Point_string(&q2->center, str2);
    sprintf(buffer, "get_quadrant(%s, %s)", str1, str2);
    assertLong(0, (unsigned long long)get_quadrant(&square1->center, &q2->center), buffer);

    printf("\n---Quadtree_add Inner Square Generation Test---\n");
    assertTrue(Quadtree_add(q1, p4), "Quadtree_add(q1, p4)");
    Node *square2 = NULL;
    if (square1->children[get_quadrant(&square1->center, &p4)] != NULL) {
        square2 = square1->children[get_quadrant(&square1->center, &p4)];
        for (i = 0; i < D; i++) coords[i] = 2;
        assertPoint(Point_from_array(coords), square2->center, "square2->center");

        if (square2->children[get_quadrant(&square2->center, &p4)] != NULL) {
            sprintf(buffer, "square2->children[%llu]->center", (unsigned long long)get_quadrant(&square2->center, &p4));
            assertPoint(p4, square2->children[get_quadrant(&square2->center, &p4)]->center, buffer);
        }
        else {
            sprintf(buffer, "square2->children[%llu]->center is not NULL", (unsigned long long)get_quadrant(&square2->center, &p4));
            assertError(buffer);
        }
    }
    else {  // alert to problems
        sprintf(buffer, "square1->children[%llu] is not NULL", (unsigned long long)get_quadrant(&square1->center, &p4));
        assertError(buffer);
        sprintf(buffer, "square1->children[%llu]->center is not NULL", (unsigned long long)get_quadrant(&square1->center, &p4));
        assertError(buffer);
    }

    printf("\n---Quadtree_add Greater Depth Test---\n");
    assertTrue(Quadtree_add(q1, p5), "Quadtree_add(q1, p5)");
    sprintf(buffer, "(q1->children[%llu] != NULL)", (unsigned long long)get_quadrant(&q1->center, &p5));
    assertTrue(q1->children[get_quadrant(&q1->center, &p5)] != NULL, buffer);
    if (q1->children[get_quadrant(&q1->center, &p5)] != NULL) {
        sprintf(buffer, "q1->children[%llu]->is_square", (unsigned long long)get_quadrant(&q1->center, &p5));
        assertFalse(q1->children[get_quadrant(&q1->center, &p5)]->is_square, buffer);
        sprintf(buffer, "q1->children[%llu]->center", (unsigned long long)get_quadrant(&q1->center, &p5));
        for (i = 0; i < D; i++) coords[i] = -2;
        assertPoint(Point_from_array(coords), q1->children[get_quadrant(&q1->center, &p5)]->center, buffer);
    }
    else {
        sprintf(buffer, "q1->children[%llu]->is_square is not NULL", (unsigned long long)get_quadrant(&q1->center, &p5));
        assertError(buffer);
        sprintf(buffer, "q1->children[%llu]->center is not NULL", (unsigned long long)get_quadrant(&q1->center, &p5));
        assertError(buffer);
    }

    printf("\n---Quadtree_add Alternating Quadrant Test---\n");
    assertTrue(Quadtree_add(q1, p6), "Quadtree_add(q1, p6)");
    Node *square3 = NULL;
    if (square2 != NULL && square2->children[get_quadrant(&square2->center, &p6)] != NULL) {
        square3 = square2->children[get_quadrant(&square2->center, &p6)];
        for (i = 0; i < D; i++) coords[i] = 1;
        assertPoint(Point_from_array(coords), square3->center, "square3->center");
    }
    else
        assertError("square3->center is not NULL");

    if (square3 != NULL && square3->children[get_quadrant(&square3->center, &p2)] != NULL) {
        sprintf(buffer, "square3->children[%llu]->center", (unsigned long long)get_quadrant(&square3->center, &p2));
        assertPoint(p2, square3->children[get_quadrant(&square3->center, &p2)]->center, buffer);
    }
    else {
        sprintf(buffer, "square3->children[%llu]->center is not NULL", (unsigned long long)get_quadrant(&square3->center, &p2));
        assertError(buffer);
    }

    if (square3 != NULL && square3->children[get_quadrant(&square3->center, &p6)] != NULL) {
        sprintf(buffer, "square3->children[%llu]->center", (unsigned long long)get_quadrant(&square3->center, &p6));
        assertPoint(p6, square3->children[get_quadrant(&square3->center, &p6)]->center, buffer);
    }
    else {
        sprintf(buffer, "square3->children[%llu]->center is not NULL", (unsigned long long)get_quadrant(&square3->center, &p6));
        assertError(buffer);
    }

    if (square3 != NULL && square3->children[get_quadrant(&square3->center, &p6)] != NULL &&
            square3->children[get_quadrant(&square3->center, &p6)]->up != NULL) {
        sprintf(buffer, "square3->children[%llu]->up->center", (unsigned long long)get_quadrant(&square3->center, &p6));
        assertPoint(p6, square3->children[get_quadrant(&square3->center, &p6)]->up->center, buffer);
    }
    else {
        sprintf(buffer, "square3->children[%llu]->up->center is not NULL", (unsigned long long)get_quadrant(&square3->center, &p6));
        assertError(buffer);
    }

    if (square3 != NULL && square3->up != NULL) {
        if (square3->up->children[get_quadrant(&square3->up->center, &p6)] != NULL) {
            sprintf(buffer, "square3->up->children[%llu]->center", (unsigned long long)get_quadrant(&square3->up->center, &p6));
            assertPoint(p6, square3->up->children[get_quadrant(&square3->up->center, &p6)]->center, buffer);
        }
        else {
            sprintf(buffer, "square3->up->children[%llu]->center is not NULL", (unsigned long long)get_quadrant(&square3->up->center, &p6));
            assertError(buffer);
        }
    }
    else {  // if we get here, it means that our parent-finding algorithm is wrong
        assertError("square3->up is not NULL");
    }

    QuadtreeFreeResult res = Quadtree_free(q1);
}

void test_quadtree_search() {
    register uint64_t i;

    float64_t coords[D];
    char buffer[1000];

    float64_t s1 = 16.0;  // size1; chose to use S instead of L
    for (i = 0; i < D; i++) coords[i] = 0;
    Point p1 = Point_from_array(coords);
    Quadtree *q1 = Quadtree_init(s1, p1);

    // engineer our own "random" values
    uint32_t rand_food[32] = {0, 0, 0, 0, 99, 0, 0, 99, 0, 0, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};
    test_rand_feed(rand_food, 32);

    for (i = 0; i < D; i++) coords[i] = 1;
    Point p2 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 3;
    Point p3 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 2.5;
    Point p4 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = -2;
    Point p5 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = -2.1;
    Point p6 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 1.25;
    Point p7 = Point_from_array(coords);

    assertTrue(Quadtree_add(q1, p2), "Quadtree_add(q1, p2)");
    assertTrue(Quadtree_add(q1, p3), "Quadtree_add(q1, p3)");
    assertTrue(Quadtree_add(q1, p4), "Quadtree_add(q1, p4)");
    assertTrue(Quadtree_add(q1, p5), "Quadtree_add(q1, p5)");
    assertTrue(Quadtree_add(q1, p6), "Quadtree_add(q1, p6)");
    assertTrue(Quadtree_add(q1, p7), "Quadtree_add(q1, p7)");

    assertFalse(Quadtree_search(q1, p1), "Quadtree_search(q1, p1)");  // because p1 isn't actually a data point
    assertTrue(Quadtree_search(q1, p2), "Quadtree_search(q1, p2)");
    assertTrue(Quadtree_search(q1, p3), "Quadtree_search(q1, p3)");
    assertTrue(Quadtree_search(q1, p4), "Quadtree_search(q1, p4)");
    assertTrue(Quadtree_search(q1, p5), "Quadtree_search(q1, p5)");
    assertTrue(Quadtree_search(q1, p6), "Quadtree_search(q1, p6)");
    assertTrue(Quadtree_search(q1, p7), "Quadtree_search(q1, p7)");

    Quadtree_free(q1);
}

void test_quadtree_remove() {
    register uint64_t i;

    float64_t coords[D];
    char buffer[1000];

    float64_t s1 = 16.0;  // size1; chose to use S instead of L
    for (i = 0; i < D; i++) coords[i] = 0;
    Point p1 = Point_from_array(coords);
    Quadtree *q1 = Quadtree_init(s1, p1);

    // engineer our own "random" values
    uint32_t rand_food[32] = {0, 0, 0, 0, 99, 0, 0, 99, 0, 0, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};
    test_rand_feed(rand_food, 32);

    for (i = 0; i < D; i++) coords[i] = 1;
    Point p2 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 3;
    Point p3 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 2.5;
    Point p4 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = -2;
    Point p5 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = -2.1;
    Point p6 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 1.25;
    Point p7 = Point_from_array(coords);

    assertTrue(Quadtree_add(q1, p2), "Quadtree_add(q1, p2)");
    assertTrue(Quadtree_add(q1, p3), "Quadtree_add(q1, p3)");
    assertTrue(Quadtree_add(q1, p4), "Quadtree_add(q1, p4)");
    assertTrue(Quadtree_add(q1, p5), "Quadtree_add(q1, p5)");
    assertTrue(Quadtree_add(q1, p6), "Quadtree_add(q1, p6)");
    assertTrue(Quadtree_add(q1, p7), "Quadtree_add(q1, p7)");

    assertTrue(Quadtree_search(q1, p2), "Quadtree_search(q1, p2)");
    Quadtree_remove(q1, p2);
    assertFalse(Quadtree_search(q1, p2), "Quadtree_search(q1, p2)");

    assertTrue(Quadtree_search(q1, p3), "Quadtree_search(q1, p3)");
    Quadtree_remove(q1, p3);
    assertFalse(Quadtree_search(q1, p3), "Quadtree_search(q1, p3)");

    assertTrue(Quadtree_search(q1, p4), "Quadtree_search(q1, p4)");
    Quadtree_remove(q1, p4);
    assertFalse(Quadtree_search(q1, p4), "Quadtree_search(q1, p4)");

    assertTrue(Quadtree_search(q1, p5), "Quadtree_search(q1, p5)");
    Quadtree_remove(q1, p5);
    assertFalse(Quadtree_search(q1, p5), "Quadtree_search(q1, p5)");

    assertTrue(Quadtree_search(q1, p6), "Quadtree_search(q1, p6)");
    Quadtree_remove(q1, p6);
    assertFalse(Quadtree_search(q1, p6), "Quadtree_search(q1, p6)");

    assertTrue(Quadtree_search(q1, p7), "Quadtree_search(q1, p7)");
    Quadtree_remove(q1, p7);
    assertFalse(Quadtree_search(q1, p7), "Quadtree_search(q1, p7)");

    Quadtree_free(q1);
}

void test_randomized() {
    register uint64_t i;

    float64_t coords[D];
    char buffer[1000];

    float64_t s1 = 16.0;  // size1; chose to use S instead of L
    for (i = 0; i < D; i++) coords[i] = 0;
    Point p1 = Point_from_array(coords);
    Quadtree *q1 = Quadtree_init(s1, p1);

    test_rand_off();

    for (i = 0; i < D; i++) coords[i] = 1;
    Point p2 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 3;
    Point p3 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 2.5;
    Point p4 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = -2;
    Point p5 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = -2.1;
    Point p6 = Point_from_array(coords);
    for (i = 0; i < D; i++) coords[i] = 1.25;
    Point p7 = Point_from_array(coords);

    assertTrue(Quadtree_add(q1, p2), "Quadtree_add(q1, p2)");
    assertTrue(Quadtree_add(q1, p3), "Quadtree_add(q1, p3)");
    assertTrue(Quadtree_add(q1, p4), "Quadtree_add(q1, p4)");
    assertTrue(Quadtree_add(q1, p5), "Quadtree_add(q1, p5)");
    assertTrue(Quadtree_add(q1, p6), "Quadtree_add(q1, p6)");
    assertTrue(Quadtree_add(q1, p7), "Quadtree_add(q1, p7)");

    assertTrue(Quadtree_search(q1, p2), "Quadtree_search(q1, p2)");
    Quadtree_remove(q1, p2);
    assertFalse(Quadtree_search(q1, p2), "Quadtree_search(q1, p2)");

    assertTrue(Quadtree_search(q1, p3), "Quadtree_search(q1, p3)");
    Quadtree_remove(q1, p3);
    assertFalse(Quadtree_search(q1, p3), "Quadtree_search(q1, p3)");

    assertTrue(Quadtree_search(q1, p4), "Quadtree_search(q1, p4)");
    Quadtree_remove(q1, p4);
    assertFalse(Quadtree_search(q1, p4), "Quadtree_search(q1, p4)");

    assertTrue(Quadtree_search(q1, p5), "Quadtree_search(q1, p5)");
    Quadtree_remove(q1, p5);
    assertFalse(Quadtree_search(q1, p5), "Quadtree_search(q1, p5)");

    assertTrue(Quadtree_search(q1, p6), "Quadtree_search(q1, p6)");
    Quadtree_remove(q1, p6);
    assertFalse(Quadtree_search(q1, p6), "Quadtree_search(q1, p6)");

    assertTrue(Quadtree_search(q1, p7), "Quadtree_search(q1, p7)");
    Quadtree_remove(q1, p7);
    assertFalse(Quadtree_search(q1, p7), "Quadtree_search(q1, p7)");

    Quadtree_free(q1);
}

void test_performance() {
    register uint64_t i, j;

    float64_t coords[D];
    char buffer[1000];

    float64_t s1 = 1 << 16;  // size1; chose to use S instead of L
    for (i = 0; i < D; i++) coords[i] = 0;
    Point p1 = Point_from_array(coords);
    Quadtree *q1 = Quadtree_init(s1, p1);

    test_rand_off();

    const uint64_t num_samples = 1L << 17;

    float64_t time_samples[num_samples];
    uint64_t total_cycles = 0;

    for (i = 0; i < num_samples; i++) {
        for (j = 0; j < D; j++) coords[j] = (Marsaglia_random() - 0.5) * s1;
        Point p = Point_from_array(coords);
        clock_t start = clock();
        bool result = Quadtree_add(q1, p);
        clock_t end = clock();
        if (result) {
            time_samples[i] = ((float64_t)(end - start)); // / CLOCKS_PER_SEC;
            total_cycles += (end - start);
        }
        else
            i--;
    }

    double time_samples_d1[num_samples - 1];  // first derivatives
    for (i = 0; i < num_samples - 1; i++) {
        time_samples_d1[i] = time_samples[i + 1] - time_samples[i];
    }

    printf("Total time for %llu inserts: %.8lf s\n", (unsigned long long)num_samples, total_cycles / (float64_t)CLOCKS_PER_SEC);

    Quadtree_free(q1);
}

int main(int argc, char *argv[]) {
    setbuf(stdout, 0);
    mtrace();
    Marsaglia_srand(time(NULL));
    printf("[Beginning tests]\n");
    
    start_test(test_sizes, "Struct sizes");
    start_test(test_in_range, "in_range");
    start_test(test_get_quadrant, "get_quadrant");
    start_test(test_get_new_center, "get_new_center");
    start_test(test_quadtree_create, "Quadtree_init");
    start_test(test_quadtree_add, "Quadtree_add");
    start_test(test_quadtree_search, "Quadtree_search");
    start_test(test_quadtree_remove, "Quadtree_remove");
    start_test(test_randomized, "Randomized (in-environment)");
    //start_test(test_performance, "Performance tests");

    printf("\n[Ending tests]\n");
    printf("\033[7;33m=============================================\n");
    printf("         TESTS AND ASSERTIONS REPORT         \n");
    printf("              DIMENSIONS: %5llu              \n", (unsigned long long)D);
    printf("=============================================\033[m\n");
    printf("\033[1;36mTOTAL  TESTS: %4lld\033\[m | \033\[1;36mTOTAL  ASSERTIONS: %5lld\033\[m\n", total_tests(), total_assertions());
    printf("\033[3;32mPASSED TESTS: %4lld\033\[m | \033\[3;32mPASSED ASSERTIONS: %5lld\033\[m\n", passed_tests(), passed_assertions());
    printf("\033[3;31mFAILED TESTS: %4lld\033\[m | \033\[3;31mFAILED ASSERTIONS: %5lld\033\[m\n", total_tests() - passed_tests(), total_assertions() - passed_assertions());
    printf("=============================================\n");
    return 0;
}
