/**
Naive serial implementation of compressed skip quadtree
*/

#include <assert.h>
#include <stdlib.h>

#include "../types.h"
#include "../Quadtree.h"
#include "../Point.h"

// rlu_self, included to make compiler happy
__thread rlu_thread_data_t *rlu_self = NULL;

// rand() functions
#ifdef QUADTREE_TEST
extern uint32_t test_rand();
#define rand() test_rand()
#else
extern uint32_t Marsaglia_rand();
#define rand() Marsaglia_rand()
extern void Marsaglia_srand(uint32_t);
#define srand(x) Marsaglia_srand(x)
#endif

// quadtree counter
#ifdef QUADTREE_TEST
uint64_t QUADTREE_NODE_COUNT = 0;
#endif

Node* Node_init(const float64_t length, const Point center) {
    Node *node = (Node*)malloc(sizeof(Node));
    node->is_square = false;
    node->length = length;
    node->center = center;
    node->parent = NULL;
    node->up = NULL;
    node->down = NULL;
    uint64_t i;
    for(i = 0; i < (1LL << D); i++) {
        node->children[i] = NULL;
    }
#ifdef QUADTREE_TEST
    node->id = QUADTREE_NODE_COUNT++;
#endif
    return node;
}

Quadtree* Quadtree_init(const float64_t length, const Point center) {
    Quadtree *qtree = Node_init(length, center);
    qtree->is_square = true;
    return qtree;
}

/*
 * Node_free
 *
 * Frees the memory used to represent this node.
 *
 * node - the node to be freed
 */
static inline void Node_free(Node * const node) {
    free((void*)node);
}

/*
 * Quadtree_search_helper
 *
 * Recursive helper function to traverse the tree horizontally before dropping levels.
 *
 * Invariant: node is always a square.
 *
 * node - the square to look in
 * p - the point to search for
 *
 * Returns whether p is in node.
 */
bool Quadtree_search_helper(Node * const node, const Point *p) {
    if (!in_range(node, p))
        return false;

    register uint64_t quadrant = get_quadrant(&node->center, p);

    // if the target child is NULL, we try to drop down a level
    if (node->children[quadrant] == NULL) {
        if (node->down != NULL)
            return Quadtree_search_helper(node->down, p);
        // otherwise, we're on the bottom-most level and just can't find the point
        else
            return false;
    }

    // here, the child exists

    // if is a square, move to it and recurse
    if (node->children[quadrant]->is_square)
        return Quadtree_search_helper(node->children[quadrant], p);

    // otherwise, we check if the child point matches, since it's a point node
    if (Point_equals(&node->children[quadrant]->center, p))
        return true;

    // if we're here, then we need to branch down a level
    if (node->down != NULL)
        return Quadtree_search_helper(node->down, p);

    // here, we have nowhere else to search for, so we give up
    return false;
}

bool Quadtree_search(const Quadtree * const node, const Point p) {
    Node *current = (Node*)node;

    if (current == NULL)
        return false;

    while (current->up != NULL)
        current = current->up;

    return Quadtree_search_helper(current, &p);
}

/*
 * Quadtree_add_helper
 *
 * Recursive helper function to add new points to the tree.
 *
 * The process is three-part:
 * 1. We traverse node on the topmost level to where p should be added.
 * 2. We then branch down to create lower-level nodes first.
 * 3. We then take the lower-level node and use it as our down for this level.
 *
 * node - the node to start inserting at; should be a square
 * p - the point to add
 * gap_depth - the number of levels we need to go through before actually inserting nodes
 *
 * Returns the corresponding node, one level lower, or NULL if the action failed.
 */
Node* Quadtree_add_helper(Node * node, const Point * const p, const uint64_t gap_depth) {
    if (!in_range(node, p))
        return NULL;

    // horizontal traversal
    Node *parent, *node_param = node;
    do {
        parent = node;
        node = parent->children[get_quadrant(&parent->center, p)];
    } while(node != NULL && node->is_square && in_range(node, p));

    // check for duplication
    if (!gap_depth && node != NULL && !node->is_square && Point_equals(&node->center, p))
        return NULL;

    // branch down a level if possible
    Node *down_node = NULL;
    if (parent->down != NULL)
        if ((down_node = Quadtree_add_helper(parent->down, p,
                (gap_depth > 0) & (gap_depth - 1))) == NULL)
            return NULL;

    // if gap_depth is not zero, we shouldn't actually add anything
    if (gap_depth)
        return down_node;

    Node *new_node = Node_init(0.5 * parent->length, *p);
    new_node->parent = parent;

    if (down_node != NULL) {
        new_node->down = down_node;
        down_node->up = new_node;
    }

    // time to try inserting onto this level
    register uint64_t quadrant = get_quadrant(&parent->center, p);

    // if the slot is empty, it's trivial
    if (parent->children[quadrant] == NULL) {
        parent->children[quadrant] = new_node;
        return new_node;
    }
    // if it's not empty, that means there's already a node there...
    else {
        // grab the sibling-to-be
        Node *sibling = parent->children[quadrant];

        // create a new square to contain the sibling and the new node
        uint64_t square_quadrant = quadrant;
        Node *square = Quadtree_init(0.5 * parent->length, get_new_center(parent, quadrant));
        square->parent = parent;

        // now, we keep splitting until the new node and the sibling are in different quadrants
        register uint64_t sibling_quadrant;
        while ( (sibling_quadrant = get_quadrant(&square->center, &sibling->center)) ==
                (quadrant = get_quadrant(&square->center, &new_node->center))) {
            Point new_square_center = get_new_center(square, quadrant);
            Point_copy(&new_square_center, &square->center);
            square->length *= 0.5;
        }

        // okay, now we have separate quadrants to use
        square->children[quadrant] = new_node;
        square->children[sibling_quadrant] = sibling;

        // now, we need to find the down square to this square, if we're not on the last level
        if (parent->down != NULL) {
            Node *down_square = parent->down;
            while (!Point_equals(&down_square->center, &square->center) ||
                    abs(down_square->length - square->length) > PRECISION)
                down_square = down_square->children[get_quadrant(&down_square->center, &square->center)];
            square->down = down_square;
            down_square->up = square;
        }

        square->parent->children[square_quadrant] = square;
        new_node->parent = square;
        sibling->parent = square;
    }

    return new_node;
}

bool Quadtree_add(Quadtree * const node, const Point p) {
    Node *current = node;

    while (rand() % 100 < 50) {
        if (current->up == NULL) {
            current->up = Quadtree_init(current->length, current->center);
            current->up->down = current;
        }
        current = current->up;
    }
    
    register uint64_t gap_depth = 0;  // number of layers to ignore when inserting

    while (current->up != NULL) {
        gap_depth++;
        current = current->up;
    }

    return Quadtree_add_helper(current, &p, gap_depth) != NULL;
}

/*
 * Quadtree_remove_node
 *
 * Helper function to remove all instances of the given node and properly relink pointers
 * to it.
 *
 * Invariant: node must have either a parent or a down.
 *
 * node - the node to remove
 *
 * Returns true if removal is successful, false otherwise.
 */
bool Quadtree_remove_node(Node * const node) {
    if (node->down == NULL && node->parent == NULL)
        return false;

    // if is square, determine whether need to remove, and if so, which node to move up
    if (node->is_square) {
        register uint64_t num_children = 0, i;
        Node *child = NULL;
        for (i = 0; i < (1LL << D); i++)
            if (node->children[i] != NULL) {
                num_children++;
                child = node->children[i];
            }

        // cannot remove square if more than 1 child
        if (num_children > 1)
            return false;

        // if we have a child, then we relink parent to point to this child, and unlink
        // ourself from the parent
        if (num_children == 1) {
            // however this cannot happen at the root. If we're going to remove a copy of
            // the root, we want no children on that node
            if (node->parent == NULL)
                return false;

            // if all goes well, we can relink
            node->parent->children[get_quadrant(&node->parent->center, &node->center)] = child;
            child->parent = node->parent;
            node->parent = NULL;
        }

        // otherwise, 0 children, and no problem
    }

    // now, get rid of pointers from the parent
    Node *parent = node->parent, *up = node->up, *down = node->down;
    if (parent != NULL && parent->children[get_quadrant(&parent->center, &node->center)] == node)
        parent->children[get_quadrant(&parent->center, &node->center)] = NULL;

    // next, unlink pointers from up and down
    if (node->up != NULL) {
        node->up->down = NULL;
        node->up = NULL;
    }
    if (node->down != NULL) {
        node->down->up = NULL;
        node->down = NULL;
    }

    // now, we can get rid of our node
    Node_free(node);

    // then, recurse up the parent as necessary
    if (parent != NULL) {
        register uint64_t num_children = 0, i;
        for (i = 0; i < (1LL << D); i++)
            num_children += parent->children[i] != NULL;
        if (num_children < 2)
            Quadtree_remove_node(parent);
    }

    // finally, recurse on up and down
    if (up != NULL)
        Quadtree_remove_node(up);
    if (down != NULL)
        Quadtree_remove_node(down);

    return true;
}

/*
 * Quadtree_remove_helper
 *
 * Recursive helper function to remove nodes. Removal starts at highest-level occurance
 * and progresses downward.
 *
 * node - the node to start at
 * p - the point to remove
 *
 * Returns true if the node was successfully removed, false if not.
 */
bool Quadtree_remove_helper(Node * const node, const Point * const p) {
    if (!in_range(node, p))
        return false;

    register uint64_t quadrant = get_quadrant(&node->center, p);

    // if the target child is NULL, we try to drop down a level
    if (node->children[quadrant] == NULL) {
        if (node->down != NULL)
            return Quadtree_remove_helper(node->down, p);
        // otherwise, we're on the bottom-most level and just can't find the point
        else
            return false;
    }

    // here, the child exists

    // if is a square, move to it and recurse if in range
    if (node->children[quadrant]->is_square && in_range(node->children[quadrant], p))
        return Quadtree_remove_helper(node->children[quadrant], p);

    // otherwise, we check if the child point matches, since it's a point node
    if (Point_equals(&node->children[quadrant]->center, p))
        return Quadtree_remove_node(node->children[quadrant]);

    // if we're here, then we need to branch down a level
    if (node->down != NULL)
        return Quadtree_remove_helper(node->down, p);

    // here, we have nowhere else to search for, so we give up
    return false;
 
}

bool Quadtree_remove(Quadtree * const node, const Point p) {
    Node *current = node;

    while (current->up != NULL)
        current = current->up;

    return Quadtree_remove_helper(current, &p);
}

/*
 * Quadtree_free_helper
 *
 * Recursively frees the direct children of this node. Does not deal with other levels of
 * the tree.
 *
 * Stores result information in the referenced QuadtreeFreeResult struct.
 *
 * node - the node to free
 * result - the result to write to
 *
 * Returns whether freedom of the (sub)tree start at this node was successful.
 */
bool Quadtree_free_helper(Node * const node, QuadtreeFreeResult * const result) {
    bool success = true;
    if (node->is_square) {
        register uint64_t i;
        for (i = 0; i < (1LL << D); i++)
            if (node->children[i] != NULL) {
                success &= Quadtree_free_helper(node->children[i], result);
                node->children[i] = NULL;
            }
    }

    // up and down
    if (node->up != NULL)
        node->up->down = NULL;
    if (node->down != NULL)
        node->down->up = NULL;

    result->total++;
    result->leaf += !node->is_square;

    // should no longer have references
    Node_free(node);

    return success;
}

QuadtreeFreeResult Quadtree_free(Quadtree * const root) {
    Node *current = (Node*)root;

    QuadtreeFreeResult result = (QuadtreeFreeResult){ .total = 0, .leaf = 0, .levels = 0 };

    while (current->up != NULL)
        current = current->up;

    while (current != NULL) {
        Node *next_current = current->down;
        result.levels += Quadtree_free_helper(current, &result);
        current = next_current;
    }

    return result;
}
