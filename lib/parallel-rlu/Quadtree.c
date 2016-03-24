/**
Parallel implementation of compressed skip quadtree using RLU
*/

#include <assert.h>
#include <stdlib.h>

#include "../types.h"
#include "../Quadtree.h"
#include "../Point.h"

// rlu_self
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

#define TRY_OR_FAIL(node) if (!RLU_TRY_LOCK(rlu_self, &node)) {return NULL;}
#define DEREF(node) (Node*)RLU_DEREF(rlu_self, node)

Node* Node_init(const float64_t length, const Point center) {
    Node *node = (Node*)RLU_ALLOC(sizeof(Node));
    node->is_square = false;
    node->length = length;
    node->center = center;
    node->parent = NULL;
    node->up = NULL;
    node->down = NULL;
    uint64_t i;
    for(i = 0; i < (1 << D); i++) {
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
static inline void Node_free(const Node *node) {
    RLU_FREE(rlu_self, (void*)node);
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
bool Quadtree_search_helper(const Node * const node, const Point *p) {
    Node *current = DEREF(node);

    if (!in_range(node, p))
        return false;

    register uint8_t quadrant = get_quadrant(&current->center, p);
    Node *child = DEREF(current->children[quadrant]);
    Node *down = DEREF(current->down);

    // if the target child is NULL, we try to drop down a level
    if (child == NULL) {
        if (down != NULL)
            return Quadtree_search_helper(down, p);
        // otherwise, we're on the bottom-most level and just can't find the point
        else
            return false;
    }

    // here, the child exists

    // if is a square, move to it and recurse
    if (child->is_square)
        return Quadtree_search_helper(child, p);

    // otherwise, we check if the child point matches, since it's a point node
    if (Point_equals(&child->center, p))
        return true;

    // if we're here, then we need to branch down a level
    if (down != NULL)
        return Quadtree_search_helper(down, p);

    // here, we have nowhere else to search for, so we give up
    return false;
}

bool Quadtree_search(const Quadtree * const node, const Point p) {
    RLU_READER_LOCK(rlu_self);

    Node *current = DEREF(node);

    if (current == NULL)
        return false;

    while (current->up != NULL)
        current = DEREF(current->up);

    bool found = Quadtree_search_helper(current, &p);

    RLU_READER_UNLOCK(rlu_self);

    return found;
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
 * node - the node to start inserting at; should be a square; must be the original Node
 * p - the point to add
 * gap_depth - the number of levels we need to go through before actually inserting nodes
 *
 * Returns the corresponding raw node, one level lower, or NULL if the action failed.
 */
Node* Quadtree_add_helper(const Node * const node, const Point * const p, const uint64_t gap_depth) {
    // name_node is the raw node, name is the deref'ed version

    Node *current_node = (Node*)node, *current = DEREF(current_node);
    if (!in_range(current, p))
        return NULL;

    // horizontal traversal
    Node *parent_node, *parent;
    do {
        parent_node = current_node;
        parent = current;
        current_node = parent->children[get_quadrant(&parent->center, p)];
        current = DEREF(current_node);
    } while(Node_valid(current) && current->is_square && in_range(current, p));
    TRY_OR_FAIL(parent);

    // check for duplication
    if (!gap_depth && Node_valid(current) && !current->is_square && Point_equals(&current->center, p))
        return NULL;

    // branch down a level if possible
    Node *down_node = NULL, *down = NULL;
    if (Node_valid(parent->down)) {
        if (!Node_valid(down_node = Quadtree_add_helper(parent->down, p,
                (gap_depth > 0) & (gap_depth - 1))))
            return NULL;
        down = DEREF(down_node);
    }

    // if gap_depth is not zero, we shouldn't actually add anything
    if (gap_depth)
        return down_node;

    Node *new_node = Node_init(0.5 * parent->length, *p), *new = DEREF(new_node);
    TRY_OR_FAIL(new);
    RLU_ASSIGN_PTR(rlu_self, &new->parent, parent_node);

    if (Node_valid(down)) {
        TRY_OR_FAIL(down);
        RLU_ASSIGN_PTR(rlu_self, &new->down, down_node);
        RLU_ASSIGN_PTR(rlu_self, &down->up, new_node);
    }

    // time to try inserting onto this level
    register uint8_t quadrant = get_quadrant(&parent->center, p);

    // if the slot is empty, it's trivial
    Node *sibling_node = parent->children[quadrant];
    if (!Node_valid(sibling_node)) {
        RLU_ASSIGN_PTR(rlu_self, &parent->children[quadrant], new_node);
    }
    // if it's not empty, that means there's already a node there...
    else {
        // grab the lock on the sibling-to-be
        Node *sibling = DEREF(sibling_node);
        TRY_OR_FAIL(sibling);

        // create a new square to contain the sibling and the new node
        uint8_t square_quadrant = quadrant;
        Node *square_node = Quadtree_init(0.5 * parent->length, get_new_center(parent, quadrant));
        Node *square = DEREF(square_node);
        TRY_OR_FAIL(square);
        RLU_ASSIGN_PTR(rlu_self, &square->parent, parent_node);

        // now, we keep splitting until the new node and the sibling are in different quadrants
        register uint8_t sibling_quadrant;
        while ( (sibling_quadrant = get_quadrant(&square->center, &sibling->center)) ==
                (quadrant = get_quadrant(&square->center, &new->center))) {
            Point new_square_center = get_new_center(square, quadrant);
            Point_copy(&new_square_center, &square->center);
            square->length *= 0.5;
        }

        // okay, now we have separate quadrants to use
        RLU_ASSIGN_PTR(rlu_self, &square->children[quadrant], new_node);
        RLU_ASSIGN_PTR(rlu_self, &square->children[sibling_quadrant], sibling_node);

        // now, we need to find the down square to this square, if we're not on the last level
        if (Node_valid(parent->down)) {
            Node *down_square_node = parent->down, *down_square = DEREF(down_square_node);
            while (!Point_equals(&down_square->center, &square->center) ||
                    abs(down_square->length - square->length) > PRECISION) {
                down_square_node = down_square->children[get_quadrant(&down_square->center, &square->center)];
                down_square = DEREF(down_square_node);
                if (!Node_valid(down_square))
                    return NULL;
            }
            TRY_OR_FAIL(down_square);
            RLU_ASSIGN_PTR(rlu_self, &square->down, down_square_node);
            RLU_ASSIGN_PTR(rlu_self, &down_square->up, square_node);
        }

        RLU_ASSIGN_PTR(rlu_self, &parent->children[square_quadrant], square_node);
        RLU_ASSIGN_PTR(rlu_self, &new->parent, square_node);
        RLU_ASSIGN_PTR(rlu_self, &sibling->parent, square_node);
    }

    return new_node;
}

bool Quadtree_add(Quadtree * const node, const Point p) {
    register uint8_t attempts_left = 10;
add_restart:
    RLU_READER_LOCK(rlu_self);

    Node *current_node = (Node*)node, *current = DEREF(current_node);

    while (rand() % 100 < 50) {
        if (current->up == NULL) {
            if (!RLU_TRY_LOCK(rlu_self, &current))
                goto add_abort;
            Node *up_node = Quadtree_init(current->length, current->center);
            Node *up = DEREF(up_node);
            if (!RLU_TRY_LOCK(rlu_self, &up))
                goto add_abort;
            RLU_ASSIGN_PTR(rlu_self, &up->down, current_node);
            RLU_ASSIGN_PTR(rlu_self, &current->up, up_node);
        }
        current_node = current->up;
        current = DEREF(current_node);
    }
    
    register uint64_t gap_depth = 0;  // number of layers to ignore when inserting

    while (current->up != NULL) {
        current_node = current->up;
        current = DEREF(current_node);
        gap_depth++;
    }

    bool success = Quadtree_add_helper(current_node, &p, gap_depth) != NULL;

    if (!success) {
add_abort:
        RLU_ABORT(rlu_self);
        if (--attempts_left)
            goto add_restart;
    }
    else {
        RLU_READER_UNLOCK(rlu_self);
    }

    return success;
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
bool Quadtree_remove_node(const Node * const node) {
    // name_node is the raw node, name is the deref'ed version

    Node *current_node = (Node*)node, *current = DEREF(current_node);

    // root node at lowest level, can't be removed
    if (!Node_valid(current->down) && !Node_valid(current->parent))
        return false;

    TRY_OR_FAIL(current);

    // if is square, determine whether need to remove, and if so, which node to move up
    if (current->is_square) {
        register uint8_t num_children = 0, i;
        Node *child_node = NULL;
        for (i = 0; i < (1 << D); i++)
            if (Node_valid(current->children[i])) {
                num_children++;
                child_node = current->children[i];
            }

        // cannot remove square if more than 1 child
        if (num_children > 1)
            return false;

        // if we have a child, then we relink parent to point to this child, and unlink
        // ourself from the parent
        if (num_children == 1) {
            // however this cannot happen at the root. If we're going to remove a copy of
            // the root, we want no children on that node
            if (!Node_valid(current->parent))
                return false;

            // if all goes well, we can relink
            Node *parent_node = current->parent, *parent = DEREF(parent_node);
            Node *child = DEREF(child_node);
            TRY_OR_FAIL(parent);
            TRY_OR_FAIL(child);
            RLU_ASSIGN_PTR(rlu_self, &parent->children[get_quadrant(&parent->center, &current->center)], child_node);
            RLU_ASSIGN_PTR(rlu_self, &child->parent, parent_node);
            RLU_ASSIGN_PTR(rlu_self, &current->parent, NULL);
        }

        // otherwise, 0 children, and no problem
    }

    // now, get rid of pointers from the parent
    Node *parent_node = current->parent, *parent = DEREF(parent_node);
    Node *up_node = current->up, *up = NULL;
    if (Node_valid(up_node))
        up = DEREF(up_node);
    Node *down_node = current->down, *down = NULL;
    if (Node_valid(down_node))
        down = DEREF(down_node);

    if (Node_valid(parent) && parent->children[get_quadrant(&parent->center, &current_node->center)] == current_node) {
        TRY_OR_FAIL(parent);
        RLU_ASSIGN_PTR(rlu_self, &parent->children[get_quadrant(&parent->center, &current_node->center)], NULL);
    }

    // next, unlink pointers from up and down
    if (Node_valid(up)) {
        TRY_OR_FAIL(up);
        RLU_ASSIGN_PTR(rlu_self, &up->down, NULL);
        RLU_ASSIGN_PTR(rlu_self, &current->up, NULL);
    }
    if (Node_valid(down)) {
        TRY_OR_FAIL(down);
        RLU_ASSIGN_PTR(rlu_self, &down->up, NULL);
        RLU_ASSIGN_PTR(rlu_self, &current->down, NULL);
    }

    // now, we can get rid of our node
    Node_free(current);

    // then, recurse up the parent as necessary
    if (Node_valid(parent)) {
        register uint8_t num_children = 0, i;
        for (i = 0; i < (1 << D); i++)
            num_children += Node_valid(parent->children[i]);
        if (num_children < 2)
            Quadtree_remove_node(parent_node);
    }

    // finally, recurse on up and down
    if (Node_valid(down))
        Quadtree_remove_node(down_node);

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
    // name_node is the raw node, name is the deref'ed version

    Node *current_node = (Node*)node, *current = DEREF(current_node);
    if (!in_range(current, p))
        return false;

    register uint8_t quadrant = get_quadrant(&current->center, p);

    // if the target child is NULL, we try to drop down a level
    if (!Node_valid(current->children[quadrant])) {
        if (Node_valid(current->down))
            return Quadtree_remove_helper(current->down, p);
        // otherwise, we're on the bottom-most level and just can't find the point
        else
            return false;
    }

    // here, the child exists

    // if is a square, move to it and recurse if in range
    if (current->children[quadrant]->is_square && in_range(current->children[quadrant], p))
        return Quadtree_remove_helper(current->children[quadrant], p);

    // otherwise, we check if the child point matches, since it's a point node
    if (Point_equals(&current->children[quadrant]->center, p))
        return Quadtree_remove_node(current->children[quadrant]);

    // if we're here, then we need to branch down a level
    if (Node_valid(current->down))
        return Quadtree_remove_helper(current->down, p);

    // here, we have nowhere else to search for, so we give up
    return false;
 
}

bool Quadtree_remove(Quadtree * const node, const Point p) {
    RLU_READER_LOCK(rlu_self);

    Node *current_node = (Node*)node, *current = DEREF(current_node);
    while (current->up != NULL) {
        current_node = current->up;
        current = DEREF(current_node);
    }

    bool success = Quadtree_remove_helper(current_node, &p);

    RLU_READER_UNLOCK(rlu_self);

    return success;
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
        register uint8_t i;
        for (i = 0; i < (1 << D); i++)
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
    // free is not a threadsafe call, disable RLU free list caching
    rlu_thread_data_t *old_rlu_self = rlu_self;
    rlu_self = NULL;

    Node *current = (Node*)root;

    QuadtreeFreeResult result = (QuadtreeFreeResult){ .total = 0, .leaf = 0, .levels = 0 };

    while (current->up != NULL)
        current = current->up;

    while (current != NULL) {
        Node *next_current = current->down;
        result.levels += Quadtree_free_helper(current, &result);
        current = next_current;
    }

    rlu_self = old_rlu_self;

    return result;
}
