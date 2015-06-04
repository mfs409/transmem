/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <vector>
#include <set>
#include "element.h"
#include "mesh.h"
#include "coordinate.h"
#include <queue>

struct region_t {
    coordinate_t centerCoordinate;
    std::queue<element_t*>*     expandQueuePtr;
    std::set<element_t*, element_listCompare_t>* beforeListPtr; /* before retriangulation; list to avoid duplicates */

    std::set<edge_t*, element_listCompareEdge_t>*      borderListPtr; /* edges adjacent to region; list to avoid duplicates */
    std::vector<element_t*>* badVectorPtr;

    region_t();
    ~region_t();

    /* =============================================================================
     * TMregion_refine
     *
     * Calculate refined triangles. The region is built by using a breadth-first
     * search starting from the element (elementPtr) containing the new point we
     * are adding. If expansion hits a boundary segment (encroachment) we build
     * a region around that element instead, to avoid a potential infinite loop.
     *
     * Returns net number of elements added to mesh.
     * =============================================================================
     */
    __attribute__((transaction_safe))
    long refine(element_t* elementPtr, mesh_t* meshPtr, bool* success);

    __attribute__((transaction_safe))
    void clearBad();

    __attribute__((transaction_safe))
    void transferBad(std::multiset<element_t*, element_heapCompare_t>* workHeapPtr);
};
