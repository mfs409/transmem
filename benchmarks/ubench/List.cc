/**
 *  Copyright (C) 2011, 2015
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <climits>
#include "List.h"

// constructor just makes a sentinel for the data structure
List::List() : sentinel(new Node()) { }

// simple sanity check: make sure all elements of the list are in sorted order
bool List::isSane(void) const
{
    const Node* prev(sentinel);
    const Node* curr((prev->m_next));

    while (curr != NULL) {
        if ((prev->m_val) >= (curr->m_val))
            return false;
        prev = curr;
        curr = curr->m_next;
    }
    return true;
}

// extended sanity check, does the same as the above method, but also calls v()
// on every item in the list
bool List::extendedSanityCheck(verifier v, uint32_t v_param) const
{
    const Node* prev(sentinel);
    const Node* curr((prev->m_next));
    while (curr != NULL) {
        if (!v((curr->m_val), v_param) || ((prev->m_val) >= (curr->m_val)))
            return false;
        prev = curr;
        curr = prev->m_next;
    }
    return true;
}

// insert method; find the right place in the list, add val so that it is in
// sorted order; if val is already in the list, exit without inserting
bool List::insert(int val)
{
    // traverse the list to find the insertion point
    const Node* prev(sentinel);
    const Node* curr(prev->m_next);

    while (curr != NULL) {
        if ((curr->m_val) >= val)
            break;
        prev = curr;
        curr = (prev->m_next);
    }

    // now insert new_node between prev and curr
    if (!curr || ((curr->m_val) > val)) {
        Node* insert_point = const_cast<Node*>(prev);

        // create the new node
        Node* i = (Node*)malloc(sizeof(Node));
        i->m_val = val;
        i->m_next = const_cast<Node*>(curr);
        insert_point->m_next = i;
        return true;
    }
    else {
        return false;
    }
}

// search function
bool List::lookup(int val) const
{
    bool found = false;
    const Node* curr(sentinel);
    curr = (curr->m_next);

    while (curr != NULL) {
        if ((curr->m_val) >= val)
            break;
        curr = (curr->m_next);
    }

    found = ((curr != NULL) && ((curr->m_val) == val));
    return found;
}

// findmax function
int List::findmax() const
{
    int max = -1;
    const Node* curr(sentinel);
    while (curr != NULL) {
        max = (curr->m_val);
        curr = (curr->m_next);
    }
    return max;
}

// findmin function
int List::findmin() const
{
    int min = -1;
    const Node* curr(sentinel);
    curr = (curr->m_next);
    if (curr != NULL)
        min = (curr->m_val);
    return min;
}

// remove a node if its value == val
bool List::remove(int val)
{
    // find the node whose val matches the request
    const Node* prev(sentinel);
    const Node* curr((prev->m_next));
    while (curr != NULL) {
        // if we find the node, disconnect it and end the search
        if ((curr->m_val) == val) {
            Node* mod_point = const_cast<Node*>(prev);
            mod_point->m_next = (curr->m_next);

            // delete curr...
            free(const_cast<Node*>(curr));
            return true;
        }
        else if ((curr->m_val) > val) {
            break;
        }
        prev = curr;
        curr = (prev->m_next);
    }
    return false;
}

// Update every value in the list up to the node with value == val
void List::overwrite(int val)
{
    const Node* curr(sentinel);
    curr = (curr->m_next);

    while (curr != NULL) {
        if ((curr->m_val) >= val)
            break;
        Node* wcurr = const_cast<Node*>(curr);
        wcurr->m_val = (wcurr->m_val);
        curr = (wcurr->m_next);
    }
}
