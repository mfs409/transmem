/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/* =============================================================================
 *
 * adtree.h
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * Reference:
 *
 * A. Moore and M.-S. Lee. Cached sufficient statistics for efficient machine
 * learning with large datasets. Journal of Artificial Intelligence Research 8
 * (1998), pp 67-91.
 *
 * =============================================================================
 */

#pragma once

#include <assert.h>
#include <stdlib.h>
#include "data.h"
#include "query.h"
#include <vector>

struct adtree_vary_t;
struct adtree_node_t {
    long index;
    long value;
    long count;
    std::vector<adtree_vary_t*>* varyVectorPtr;
};

struct adtree_vary_t {
    long index;
    long mostCommonValue;
    adtree_node_t* zeroNodePtr;
    adtree_node_t* oneNodePtr;
};

struct adtree_t {
    long numVar;
    long numRecord;
    adtree_node_t* rootNodePtr;
};


/* =============================================================================
 * adtree_alloc
 * =============================================================================
 */
adtree_t*
adtree_alloc ();


/* =============================================================================
 * adtree_free
 * =============================================================================
 */
void
adtree_free (adtree_t* adtreePtr);


/* =============================================================================
 * adtree_make
 * -- Records in dataPtr will get rearranged
 * =============================================================================
 */
void
adtree_make (adtree_t* adtreePtr, data_t* dataPtr);


/* =============================================================================
 * adtree_getCount
 * -- queryVector must consist of queries sorted by id
 * =============================================================================
 */
__attribute__((transaction_safe))
long
adtree_getCount (adtree_t* adtreePtr, std::vector<query_t*>* queryVectorPtr);
