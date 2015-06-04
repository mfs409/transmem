/* =============================================================================
 *
 * hashtable.c
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * Options:
 *
 * LIST_NO_DUPLICATES (default: allow duplicates)
 *
 * HASHTABLE_RESIZABLE (enable dynamically increasing number of buckets)
 *
 * HASHTABLE_SIZE_FIELD (size is explicitely stored in
 *     hashtable and not implicitly defined by the sizes of
 *     all bucket lists => more conflicts in case of parallel access)
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 *
 * ------------------------------------------------------------------------
 *
 * For the license of ssca2, please see ssca2/COPYRIGHT
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 *
 * ------------------------------------------------------------------------
 *
 * Unless otherwise noted, the following license applies to STAMP files:
 *
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */


#include <assert.h>
#include <stdlib.h>
#include "hashtable.h"
#include "list.h"
#include "pair.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * TMhashtable_iter_reset
 * =============================================================================
 */
TM_SAFE
void
TMhashtable_iter_reset (
                        hashtable_iter_t* itPtr, hashtable_t* hashtablePtr)
{
    itPtr->bucket = 0;
    TMLIST_ITER_RESET(&(itPtr->it), hashtablePtr->buckets[0]);
}


/* =============================================================================
 * hashtable_iter_hasNext
 * =============================================================================
 */
TM_SAFE
bool_t
TMhashtable_iter_hasNext (
                          hashtable_iter_t* itPtr, hashtable_t* hashtablePtr)
{
    long bucket;
    long numBucket = hashtablePtr->numBucket;
    list_t** buckets = hashtablePtr->buckets;
    list_iter_t it = itPtr->it;

    for (bucket = itPtr->bucket; bucket < numBucket; /* inside body */) {
        list_t* chainPtr = buckets[bucket];
        if (TMLIST_ITER_HASNEXT(&it)) {
            return TRUE;
        }
        /* May use dummy bucket; see allocBuckets() */
        TMLIST_ITER_RESET(&it, buckets[++bucket]);
    }

    return FALSE;
}


/* =============================================================================
 * TMhashtable_iter_next
 * =============================================================================
 */
TM_SAFE
void*
TMhashtable_iter_next (
                       hashtable_iter_t* itPtr, hashtable_t* hashtablePtr)
{
    long bucket;
    long numBucket = hashtablePtr->numBucket;
    list_t** buckets = hashtablePtr->buckets;
    list_iter_t it = itPtr->it;
    void* dataPtr = NULL;

    for (bucket = itPtr->bucket; bucket < numBucket; /* inside body */) {
        list_t* chainPtr = hashtablePtr->buckets[bucket];
        if (TMLIST_ITER_HASNEXT(&it)) {
            pair_t* pairPtr = (pair_t*)TMLIST_ITER_NEXT(&it);
            dataPtr = pairPtr->secondPtr;
            break;
        }
        /* May use dummy bucket; see allocBuckets() */
        TMLIST_ITER_RESET(&it, buckets[++bucket]);
    }

    itPtr->bucket = bucket;
    itPtr->it = it;

    return dataPtr;
}



/* =============================================================================
 * TMallocBuckets
 * -- Returns NULL on error
 * =============================================================================
 */
TM_SAFE
list_t**
TMallocBuckets (
                long numBucket, long (*comparePairs)(const pair_t*, const pair_t*))
{
    long i;
    list_t** buckets;

    /* Allocate bucket: extra bucket is dummy for easier iterator code */
    buckets = (list_t**)malloc((numBucket + 1) * sizeof(list_t*));
    if (buckets == NULL) {
        return NULL;
    }

    for (i = 0; i < (numBucket + 1); i++) {
        list_t* chainPtr =
            TMLIST_ALLOC((long (*)(const void*, const void*))comparePairs);
        if (chainPtr == NULL) {
            while (--i >= 0) {
                TMLIST_FREE(buckets[i]);
            }
            return NULL;
        }
        buckets[i] = chainPtr;
    }

    return buckets;
}



/* =============================================================================
 * TMhashtable_alloc
 * -- Returns NULL on failure
 * -- Negative values for resizeRatio or growthFactor select default values
 * =============================================================================
 */
TM_SAFE
hashtable_t*
TMhashtable_alloc (long initNumBucket,
                   ulong_t (*hash)(const void*),
                   long (*comparePairs)(const pair_t*, const pair_t*),
                   long resizeRatio,
                   long growthFactor)
{
    hashtable_t* hashtablePtr;

    hashtablePtr = (hashtable_t*)malloc(sizeof(hashtable_t));
    if (hashtablePtr == NULL) {
        return NULL;
    }

    hashtablePtr->buckets = TMallocBuckets(  initNumBucket, comparePairs);
    if (hashtablePtr->buckets == NULL) {
        free(hashtablePtr);
        return NULL;
    }

    hashtablePtr->numBucket = initNumBucket;
#ifdef HASHTABLE_SIZE_FIELD
    hashtablePtr->size = 0;
#endif
    hashtablePtr->hash = hash;
    hashtablePtr->comparePairs = comparePairs;
    hashtablePtr->resizeRatio = ((resizeRatio < 0) ?
                                  HASHTABLE_DEFAULT_RESIZE_RATIO : resizeRatio);
    hashtablePtr->growthFactor = ((growthFactor < 0) ?
                                  HASHTABLE_DEFAULT_GROWTH_FACTOR : growthFactor);

    return hashtablePtr;
}


/* =============================================================================
 * TMfreeBuckets
 * =============================================================================
 */
TM_SAFE
void
TMfreeBuckets (  list_t** buckets, long numBucket)
{
    long i;

    /* Extra bucket is dummy for easier iterator code */
    for (i = 0; i < numBucket+1; i++) {
        TMLIST_FREE(buckets[i]);
    }

    free(buckets);
}



/* =============================================================================
 * TMhashtable_free
 * =============================================================================
 */
TM_SAFE
void
TMhashtable_free (  hashtable_t* hashtablePtr)
{
    TMfreeBuckets(  hashtablePtr->buckets, hashtablePtr->numBucket);
    free(hashtablePtr);
}



/* =============================================================================
 * TMhashtable_isEmpty
 * =============================================================================
 */
TM_SAFE
bool_t
TMhashtable_isEmpty (  hashtable_t* hashtablePtr)
{
#ifdef HASHTABLE_SIZE_FIELD
    return ((TM_SHARED_READ(hashtablePtr->size) == 0) ? TRUE : FALSE);
#else
    long i;

    for (i = 0; i < hashtablePtr->numBucket; i++) {
        if (!TMLIST_ISEMPTY(hashtablePtr->buckets[i])) {
            return FALSE;
        }
    }

    return TRUE;
#endif
}



/* =============================================================================
 * TMhashtable_getSize
 * -- Returns number of elements in hash table
 * =============================================================================
 */
TM_SAFE
long
TMhashtable_getSize (  hashtable_t* hashtablePtr)
{
#ifdef HASHTABLE_SIZE_FIELD
    return (long)TM_SHARED_READ(hashtablePtr->size);
#else
    long i;
    long size = 0;

    for (i = 0; i < hashtablePtr->numBucket; i++) {
        size += TMLIST_GETSIZE(hashtablePtr->buckets[i]);
    }

    return size;
#endif
}


/* =============================================================================
 * TMhashtable_containsKey
 * =============================================================================
 */
TM_SAFE
bool_t
TMhashtable_containsKey (  hashtable_t* hashtablePtr, void* keyPtr)
{
    pair_t* pairPtr;
    pair_t findPair;
    unsigned long i;
    //ulong_t (*hash)(const void*) TM_IFUNC_DECL = hashtablePtr->hash;
    ulong_t (*hash)(const void*) TM_SAFE = hashtablePtr->hash;

    TM_IFUNC_CALL1(i, hash, keyPtr);
    i = i % hashtablePtr->numBucket;

    findPair.firstPtr = keyPtr;
    pairPtr = (pair_t*)TMLIST_FIND(hashtablePtr->buckets[i], &findPair);

    return ((pairPtr != NULL) ? TRUE : FALSE);
}



/* =============================================================================
 * TMhashtable_find
 * -- Returns NULL on failure, else pointer to data associated with key
 * =============================================================================
 */
TM_SAFE
void*
TMhashtable_find (  hashtable_t* hashtablePtr, void* keyPtr)
{
    pair_t* pairPtr;
    pair_t findPair;
    unsigned long i;
    //ulong_t (*hash)(const void*) TM_IFUNC_DECL = hashtablePtr->hash;
    ulong_t (*hash)(const void*) TM_SAFE = hashtablePtr->hash;

    TM_IFUNC_CALL1(i, hash, keyPtr);
    i = i % hashtablePtr->numBucket;

    findPair.firstPtr = keyPtr;
    pairPtr = (pair_t*)TMLIST_FIND(hashtablePtr->buckets[i], &findPair);
    if (pairPtr == NULL) {
        return NULL;
    }

    return pairPtr->secondPtr;
}


#if defined(HASHTABLE_RESIZABLE)
#  warning "The hash table resizing must be disabled for TM"
#endif /* HASHTABLE_RESIZABLE */


/* =============================================================================
 * TMhashtable_insert
 * =============================================================================
 */
TM_SAFE
bool_t
TMhashtable_insert (
                    hashtable_t* hashtablePtr, void* keyPtr, void* dataPtr)
{
  //ulong_t (*hash)(const void*) TM_IFUNC_DECL = hashtablePtr->hash;
    ulong_t (*hash)(const void*) TM_SAFE = hashtablePtr->hash;
    long numBucket = hashtablePtr->numBucket;
    unsigned long i;

    TM_IFUNC_CALL1(i, hash, keyPtr);
    i = i % numBucket;

    pair_t findPair;
    findPair.firstPtr = keyPtr;
    pair_t* pairPtr = (pair_t*)TMLIST_FIND(hashtablePtr->buckets[i], &findPair);
    if (pairPtr != NULL) {
        return FALSE;
    }

    pair_t* insertPtr = TMPAIR_ALLOC(keyPtr, dataPtr);
    if (insertPtr == NULL) {
        return FALSE;
    }

    /* Add new entry  */
    if (TMLIST_INSERT(hashtablePtr->buckets[i], insertPtr) == FALSE) {
        TMPAIR_FREE(insertPtr);
        return FALSE;
    }

#ifdef HASHTABLE_SIZE_FIELD
    long newSize = TM_SHARED_READ(hashtablePtr->size) + 1;
    assert(newSize > 0);
    TM_SHARED_WRITE(hashtablePtr->size, newSize);
#endif

    return TRUE;
}



/* =============================================================================
 * TMhashtable_remove
 * -- Returns TRUE if successful, else FALSE
 * =============================================================================
 */
TM_SAFE
bool_t
TMhashtable_remove (  hashtable_t* hashtablePtr, void* keyPtr)
{
    long numBucket = hashtablePtr->numBucket;
    //ulong_t (*hash)(const void*) TM_IFUNC_DECL = hashtablePtr->hash;
    ulong_t (*hash)(const void*) TM_SAFE = hashtablePtr->hash;
    unsigned long i;
    list_t* chainPtr;
    pair_t* pairPtr;
    pair_t removePair;

    TM_IFUNC_CALL1(i, hash, keyPtr);
    i = i % numBucket;
    chainPtr = hashtablePtr->buckets[i];

    removePair.firstPtr = keyPtr;
    pairPtr = (pair_t*)TMLIST_FIND(chainPtr, &removePair);
    if (pairPtr == NULL) {
        return FALSE;
    }

    bool_t status = TMLIST_REMOVE(chainPtr, &removePair);
    assert(status);
    TMPAIR_FREE(pairPtr);

#ifdef HASHTABLE_SIZE_FIELD
    TM_SHARED_WRITE(hashtablePtr->size
                    (long)TM_SHARED_READ(hashtablePtr->size)-1);
    assert(hashtablePtr->size >= 0);
#endif

    return TRUE;
}


/* =============================================================================
 * TEST_HASHTABLE
 * =============================================================================
 */
#ifdef TEST_HASHTABLE


#include <stdio.h>


static ulong_t
hash (const void* keyPtr)
{
    return ((ulong_t)(*(long*)keyPtr));
}


static long
comparePairs (const pair_t* a, const pair_t* b)
{
    return (*(long*)(a->firstPtr) - *(long*)(b->firstPtr));
}


static void
printHashtable (hashtable_t* hashtablePtr)
{
    long i;
    hashtable_iter_t it;

    printf("[");
    hashtable_iter_reset(&it, hashtablePtr);
    while (hashtable_iter_hasNext(&it)) {
        printf("%li ", *((long*)(hashtable_iter_next(&it, hashtablePtr))));
    }
    puts("]");

    /* Low-level to see structure */
    for (i = 0; i < hashtablePtr->numBucket; i++) {
        list_iter_t it;
        printf("%2li: [", i);
        list_iter_reset(&it, hashtablePtr->buckets[i]);
        while (list_iter_hasNext(&it)) {
            void* pairPtr = list_iter_next(&it, hashtablePtr->buckets[i]);
            printf("%li ", *(long*)(((pair_t*)pairPtr)->secondPtr));
        }
        puts("]");
    }
}


static void
insertInt (hashtable_t* hashtablePtr, long* data)
{
    printf("Inserting: %li\n", *data);
    hashtable_insert(hashtablePtr, (void*)data, (void*)data);
    printHashtable(hashtablePtr);
    puts("");
}


static void
removeInt (hashtable_t* hashtablePtr, long* data)
{
    printf("Removing: %li\n", *data);
    hashtable_remove(hashtablePtr, (void*)data);
    printHashtable(hashtablePtr);
    puts("");
}


int
main ()
{
    hashtable_t* hashtablePtr;
    long data[] = {3, 1, 4, 1, 5, 9, 2, 6, 8, 7, -1};
    long i;

    puts("Starting...");

    hashtablePtr = hashtable_alloc(1, &hash, &comparePairs, -1, -1);

    for (i = 0; data[i] >= 0; i++) {
        insertInt(hashtablePtr, &data[i]);
        assert(*(long*)hashtable_find(hashtablePtr, &data[i]) == data[i]);
    }

    for (i = 0; data[i] >= 0; i++) {
        removeInt(hashtablePtr, &data[i]);
        assert(hashtable_find(hashtablePtr, &data[i]) == NULL);
    }

    hashtable_free(hashtablePtr);

    puts("Done.");

    return 0;
}


#endif /* TEST_HASHTABLE */

#ifdef __cplusplus
}
#endif

/* =============================================================================
 *
 * End of hashtable.c
 *
 * =============================================================================
 */
