/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <vector>
#include "net.h"

struct data_t {
    long numVar;
    long numRecord;
    char* records; /* concatenation of all records */
    std::mt19937* randomPtr;
};


/* =============================================================================
 * data_alloc
 * =============================================================================
 */
data_t*
data_alloc (long numVar, long numCase, std::mt19937* randomPtr);


/* =============================================================================
 * data_free
 * =============================================================================
 */
void
data_free (data_t* dataPtr);


/* =============================================================================
 * data_generate
 * -- Binary variables of random PDFs
 * -- If seed is <0, do not reseed
 * -- Returns random network
 * =============================================================================
 */
net_t*
data_generate (data_t* dataPtr, long seed, long maxNumParent, long percentParent);


/* =============================================================================
 * data_getRecord
 * -- Returns NULL if invalid index
 * =============================================================================
 */
char*
data_getRecord (data_t* dataPtr, long index);


/* =============================================================================
 * data_copy
 * -- Returns false on failure
 * =============================================================================
 */
bool
data_copy (data_t* dstPtr, data_t* srcPtr);


/* =============================================================================
 * data_sort
 * -- In place
 * =============================================================================
 */
void
data_sort (data_t* dataPtr,
           long start,
           long num,
           long offset);


/* =============================================================================
 * data_findSplit
 * -- Call data_sort first with proper start, num, offset
 * -- Returns number of zeros in offset column
 * =============================================================================
 */
long
data_findSplit (data_t* dataPtr, long start, long num, long offset);
