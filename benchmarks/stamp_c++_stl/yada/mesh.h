/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <random>
#include <vector>
#include <map>
#include <set>
#include "element.h"
#include <queue>

struct mesh_t {
    element_t* rootElementPtr;
    std::queue<element_t*>* initBadQueuePtr;
    long size;
    std::set<edge_t*, element_listCompareEdge_t>* boundarySetPtr;

    mesh_t();
    ~mesh_t();

    __attribute__((transaction_safe))
    void insert(element_t* elementPtr,
                std::map<edge_t*, element_t*, element_mapCompareEdge_t>* edgeMapPtr);

    __attribute__((transaction_safe))
    void remove(element_t* elementPtr);

    __attribute__((transaction_safe))
    bool insertBoundary(edge_t* boundaryPtr);

    __attribute__((transaction_safe))
    bool removeBoundary(edge_t* boundaryPtr);

    /*
     * mesh_read: Returns number of elements read from file.
     *
     * Refer to http://www.cs.cmu.edu/~quake/triangle.html for file formats.
     */
    long read(const char* fileNamePrefix);

    /*
     * mesh_getBad: Returns NULL if none
     */
    element_t* getBad();

    void shuffleBad(std::mt19937* randomPtr);

    bool check(long expectedNumElement);
};
