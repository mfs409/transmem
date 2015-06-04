/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <utility>

struct coordinate_t {
    long x;
    long y;
    long z;

    coordinate_t(long x, long y, long z);

    // NB: no explicit destructor needed
};

bool coordinate_isEqual(coordinate_t* aPtr, coordinate_t* bPtr);

bool coordinate_comparePair(const std::pair<coordinate_t*, coordinate_t*>* aPtr,
                            const std::pair<coordinate_t*, coordinate_t*>* bPtr);

bool coordinate_areAdjacent(coordinate_t* aPtr, coordinate_t* bPtr);
