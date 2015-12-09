/**
Interface for Quadtree data structure
*/

#ifndef QUADTREE_H
#define QUADTREE_H

#include <errno.h>
#include <pthread.h>
#include <stdio.h>


#include "types.h"
#include "util.h"
#include "Point.h"

#ifdef PARALLEL
typedef struct SerialSkipQuadtreeNode_t Node;
#else
typedef struct SerialSkipQuadtreeNode_t Node;
#endif
typedef Node Quadtree;

extern __thread rlu_thread_data_t *rlu_self;

/*
 * struct SerialSkipQuadtreeNode_t
 *
 * Stores information about a node in the quadtree
 *
 * is_square - true if node is a square, false if is a point
 * center - center of the square, or coordinates of the point
 * length - side length of the square. This means that the boundaries are
 *     length/2 distance from the center-> Does not matter for a point
 * parent - the parent node in the same level; NULL if a root node
 * up - the clone of the same node in the next level, if it exists
 * down - the clone of the same node in the previous level; NULL if at lowest level
 * children - the four children of the square; each entry is NULL if there is no child
 *     there. Each index refers to a quadrant, such that children[0] is Q1, [1] is Q2,
 *     and so on. Should never be all NULL unless node is a point
 */
struct SerialSkipQuadtreeNode_t {
    bool is_square;
    Point center;
    float64_t length;
    Node *parent;
    Node *up, *down;
    Node *children[1LL << D];
#ifdef QUADTREE_TEST
    uint64_t id;
#endif
};

/*
 * struct QuadtreeFreeResult_t
 *
 * Contains information about how many total nodes were freed and how many leaf nodes
 * were freed during a freeing operation. In addition, also contains information about
 * how many levels were freed.
 *
 * total - the total number of nodes freed, including intermediate and dirty nodes
 * clean - the total number of non-dirty nodes freed
 * leaf - the number of non-dirty leaf nodes freed
 * levels - the number of non-dirty levels freed
 */
typedef struct QuadtreeFreeResult_t {
    uint64_t total, clean, leaf, levels;
} QuadtreeFreeResult;

#ifdef QUADTREE_TEST
/*
 * Node_init
 *
 * Allocates memory for and initializes an empty leaf node in the quadtree.
 *
 * length - the "length" of the node -- irrelevant for a leaf node, but necessary
 *          for an internal node (square)
 * center - the center of the node
 *
 * Returns a pointer to the created node.
 */
Node* Node_init(float64_t length, Point center);
#endif

/*
 * Quadtree_init
 *
 * Allocates memory for and initializes an empty internal node in the quadtree.
 *
 * In SerialSkipQuadtree, this is implemented as a wrapper for Node_init, setting
 * the resultant node's is_square boolean to true before returning it.
 *
 * length - the "length" of the node -- irrelevant for a leaf node, but necessary
 *          for an internal node (square)
 * center - the center of the node
 *
 * Returns a pointer to the created node.
 */
Quadtree* Quadtree_init(float64_t length, Point center);

#ifdef PARALLEL
/*
 * Quadtree_parallel_search
 *
 * Alternate entry point for the search function that takes care of calling
 * pthread_create, message passing between the parent and the child threads, and storing
 * the results of the search query into a buffer.
 *
 * Can be called in a fashion similar to pthread_create, except that the function and
 * parameter arguments have been replaced with 2 parameters arguments and 1 result buffer
 * argument.
 *
 * pthread - a pthread_t reference
 * pthread_attr - a pthread_attr_t reference
 * node - the root of the tree to start searching at
 * p - the point to search for
 * ret - the result buffer to store the search result
 *
 * Returns the return value of pthread_create.
 */
int Quadtree_parallel_search(pthread_t *pthread, pthread_attr_t *pthread_attr,
        Quadtree *node, Point p, bool *ret);

/*
 * Quadtree_parallel_add
 *
 * Alternate entry point for the add function that takes care of calling
 * pthread_create, message passing between the parent and the child threads, and storing
 * the results of the add query into a buffer.
 *
 * Can be called in a fashion similar to pthread_create, except that the function and
 * parameter arguments have been replaced with 2 parameters arguments and 1 result buffer
 * argument.
 *
 * pthread - a pthread_t reference
 * pthread_attr - a pthread_attr_t reference
 * node - the root of the tree to add to
 * p - the point to add
 * ret - the result buffer to store the add result
 *
 * Returns the return value of pthread_create.
 */
int Quadtree_parallel_add(pthread_t *pthread, pthread_attr_t *pthread_attr,
        Quadtree *node, Point p, bool *ret);

/*
 * Quadtree_parallel_remove
 *
 * Alternate entry point for the remove function that takes care of calling
 * pthread_create, message passing between the parent and the child threads, and storing
 * the results of the remove query into a buffer.
 *
 * Can be called in a fashion similar to pthread_create, except that the function and
 * parameter arguments have been replaced with 2 parameters arguments and 1 result buffer
 * argument.
 *
 * pthread - a pthread_t reference
 * pthread_attr - a pthread_attr_t reference
 * node - the root of the tree to remove from
 * p - the point to remove
 * ret - the result buffer to store the remove result
 *
 * Returns the return value of pthread_create.
 */
int Quadtree_parallel_remove(pthread_t *pthread, pthread_attr_t *pthread_attr,
        Quadtree *node, Point p, bool *ret);
#endif

/*
 * Quadtree_search
 *
 * Searches for p in the quadtree pointed to by node, within a certain error tolerance.
 *
 * node - the root node to start at
 * p - the point we're searching for
 *
 * Returns whether p is in the quadtree represented by node.
 */
bool Quadtree_search(Quadtree *node, Point p);

/*
 * Quadtree_add
 *
 * Adds p to the quadtree represented by node, the root.
 *
 * Will not add duplicate points to the tree, as defined by Point_equals. If p is already
 * in the tree, this function will return false.
 *
 * node - the root node of the tree to add to
 * p - the point being added
 *
 * Returns whether the add was successful.
 */
bool Quadtree_add(Quadtree *node, Point p);

/*
 * Quadtree_remove
 *
 * Removes p from the quadtree represented by node, the root.
 *
 * node - the root node of the tree to remove from
 * p - the point being removed
 *
 * Returns whether the remove was successful: false typically indicates that the node
 * wasn't in the tree to begin with.
 */
bool Quadtree_remove(Quadtree *node, Point p);

/*bool Quadtree_search(Quadtree *node, Point p, int64_t *lock_count, uint64_t index);
bool Quadtree_add(Quadtree *node, Point p, int64_t *lock_count, uint64_t index);
bool Quadtree_remove(Quadtree *node, Point p, int64_t *lock_count, uint64_t index);*/

/*
 * Quadtree_free
 *
 * Frees the entire tree one node at a time
 *
 * The freeing order is such that, if a free fails, retrying won't
 * cause memory leaks.
 *
 * This can also be used to free subtrees.
 *
 * Returns a QuadtreeFreeResult.
 */
QuadtreeFreeResult Quadtree_free(Quadtree *root);

/*
 * Node_valid
 *
 * Returns false if node is either NULL or is dirty, and true otherwise. A value of true
 * indicates that the node is both non-NULL and not logically deleted.
 *
 * node - the node to check
 *
 * Returns whether the node is valid for use.
 */
static inline bool Node_valid(Node *node) {
    return node != NULL;
}

/*
 * in_range
 *
 * Returns true if p is within the boundaries of n, false otherwise.
 *
 * On-boundary counts as being within if on the left or bottom boundaries.
 *
 * n - the square node to check at
 * p - the point to check for
 *
 * Returns whether p is within the boundaries of n.
 */
static bool in_range(Node *n, Point *p) {
    /*return n->center->data[0] - n->length / 2 <= p->data[0] &&
        n->center->data[0] + n->length / 2 > p->data[0] &&
        n->center->data[1] - n->length / 2 <= p->data[1] &&
        n->center->data[1] + n->length / 2 > p->data[1];*/
    register float64_t bound = n->length * 0.5;
    register uint64_t i;
    for (i = 0; i < D; i++)
        if ((n->center.data[i] - bound > p->data[i]) || (n->center.data[i] + bound <= p->data[i]))
            return false;
    return true;
}

/*
 * get_quadrant
 *
 * Returns the quadrant [0,2^D) that p is in, relative to the origin point.
 *
 * Let b = quadrant id in binary, with b[0] being the least significant bit. Then, b[0] corresponds
 * to the first dimension, b[1] corresponds to the second, etc. such that b[i] corresponds to the
 * (i + 1)th dimension.
 *
 * origin - the point representing the origin of the bounding square
 * p - the point we're trying to find the quadrant of
 *
 * Returns the quadrant that p is in, relative to origin.
 */
static uint64_t get_quadrant(Point *origin, Point *p) {
    //return (p->data[0] >= origin->data[0]) + 2 * (p->data[1] >= origin->data[1]);
    register uint64_t i;
    uint64_t quadrant = 0;
    for (i = 0; i < D; i++)
        quadrant |= ((p->data[i] >= origin->data[i] - PRECISION) & 1) << i;
    return quadrant;
}

/*
 * get_new_center
 *
 * Given the current node and a quadrant, returns the Point representing
 * the center of the square that represents that quadrant.
 *
 * node - the parent node
 * quadrant - the quadrant to search for, [0, 2^D)
 *
 * Returns the center point for the given quadrant of node.
 */
static Point get_new_center(Node *node, uint64_t quadrant) {
    Point p;
    register uint64_t i;
    for (i = 0; i < D; i++)
        p.data[i] = node->center.data[i] + (((quadrant >> i) & 1) - 0.5) * 0.5 * node->length;
    return p;
}

#ifdef QUADTREE_TEST
/*
 * Node_string
 *
 * Writes value of node to the given string buffer.
 *
 * node - the node to write
 * buffer - the buffer to write to
 */
static inline void Node_string(Node *node, char *buffer) {
    char pbuf[15 * D];
    Point_string(&node->center, pbuf);
    sprintf(buffer, "Node{id = %llu, is_square = %s, center = %s, length = %lf, parent = %s, up = %s, down = %s, children = {%s",
        (unsigned long long)node->id,
        (node->is_square ? "YES" : "NO"), pbuf, node->length,
        (node->parent == NULL ? "NO" : "YES"), (node->up == NULL ? "NO" : "YES"),
        (node->down == NULL ? "NO" : "YES"),
        (node->children[0] == NULL ? "NO" : "YES"));
	uint64_t i = 1;
	for (i = 1; i < (1LL << D); i++) {
		sprintf(buffer, "%s, %s", buffer, (node->children[i] == NULL ? "NO" : "YES"));
	}
	sprintf(buffer, "%s}}", buffer);
    /*sprintf(buffer, "Node{is_square = %s, center = (%f, %f), length = %lf, parent = %p, up = %p, down = %p, children = {%p, %p, %p, %p}}",
        (node->is_square ? "YES" : "NO"), node->center->data[0], node->center->data[1], node->length,
        node->parent, node->up, node->down,
        node->children[0], node->children[1], node->children[2], node->children[3]);*/
}
#endif

// debugging function
extern void Point_string(Point*, char*);
static void print(Node *n) {
    if (n == NULL)
        printf("NULL\n");
    else {
        char pbuf[100];
        Point_string((Point*)&n->center, pbuf);
        printf("pointer = %p, is_square = %s, center = %s, length = %llu, parent = %p, up = %p, down = %p, children = {%p, %p, %p, %p}, dirty = %s, lock = %p\n", n, n->is_square ? "true" : "false", pbuf, (unsigned long long)n->length,
            !Node_valid(n->parent) ? NULL : n->parent,
            !Node_valid(n->up) ? NULL : n->up,
            !Node_valid(n->down) ? NULL : n->down,
            !Node_valid(n->children[0]) ? NULL : n->children[0],
            !Node_valid(n->children[1]) ? NULL : n->children[1],
            !Node_valid(n->children[2]) ? NULL : n->children[2],
            !Node_valid(n->children[3]) ? NULL : n->children[3],
            "false", NULL
            );
    }
}

// just for fun
#define Quadtree_plant Quadtree_init
#define Quadtree_climb Quadtree_search
#define Quadtree_grow Quadtree_add
#define Quadtree_prune Quadtree_remove
#define Quadtree_uproot Quadtree_free

#endif
