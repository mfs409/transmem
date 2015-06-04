/* =============================================================================
 *
 * region.c
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
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


#include "tm.h"
#include <assert.h>
#include <stdlib.h>
#include "region.h"
#include "coordinate.h"
#include "element.h"
#include "list.h"
#include "map.h"
#include "queue.h"
#include "mesh.h"


struct region {
    coordinate_t centerCoordinate;
    queue_t*     expandQueuePtr;
    list_t*   beforeListPtr; /* before retriangulation; list to avoid duplicates */
    list_t* borderListPtr; /* edges adjacent to region; list to avoid duplicates */
    vector_t*    badVectorPtr;
};

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */

TM_SAFE
void
TMaddToBadVector (  vector_t* badVectorPtr, element_t* badElementPtr);

TM_SAFE
long
TMretriangulate (element_t* elementPtr,
                 region_t* regionPtr,
                 mesh_t* meshPtr,
                 MAP_T* edgeMapPtr);

TM_SAFE
element_t*
TMgrowRegion (element_t* centerElementPtr,
              region_t* regionPtr,
              MAP_T* edgeMapPtr,
              bool_t* success);

/* =============================================================================
 * Pregion_alloc
 * =============================================================================
 */
region_t*
Pregion_alloc ()
{
    region_t* regionPtr;

    regionPtr = (region_t*)malloc(sizeof(region_t));
    if (regionPtr) {
        regionPtr->expandQueuePtr = TMQUEUE_ALLOC(-1);
        assert(regionPtr->expandQueuePtr);

        //[wer210] note the following compare functions should be TM_SAFE...
        regionPtr->beforeListPtr = TMLIST_ALLOC(&element_listCompare);
        assert(regionPtr->beforeListPtr);

        regionPtr->borderListPtr = TMLIST_ALLOC(&element_listCompareEdge);
        assert(regionPtr->borderListPtr);

        regionPtr->badVectorPtr = PVECTOR_ALLOC(1);
        assert(regionPtr->badVectorPtr);
    }

    return regionPtr;
}


/* =============================================================================
 * Pregion_free
 * =============================================================================
 */
void
Pregion_free (region_t* regionPtr)
{
    PVECTOR_FREE(regionPtr->badVectorPtr);
    list_free(regionPtr->borderListPtr);
    list_free(regionPtr->beforeListPtr);
    TMQUEUE_FREE(regionPtr->expandQueuePtr);
    free(regionPtr);
}


/* =============================================================================
 * TMaddToBadVector
 * =============================================================================
 */
TM_SAFE
void
TMaddToBadVector (  vector_t* badVectorPtr, element_t* badElementPtr)
{
    bool_t status = PVECTOR_PUSHBACK(badVectorPtr, (void*)badElementPtr);
    assert(status);
    TMELEMENT_SETISREFERENCED(badElementPtr, TRUE);
}


/* =============================================================================
 * TMretriangulate
 * -- Returns net amount of elements added to mesh
 * =============================================================================
 */

TM_SAFE
long
TMretriangulate (element_t* elementPtr,
                 region_t* regionPtr,
                 mesh_t* meshPtr,
                 MAP_T* edgeMapPtr)
{
    vector_t* badVectorPtr = regionPtr->badVectorPtr; /* private */
    list_t* beforeListPtr = regionPtr->beforeListPtr; /* private */
    list_t* borderListPtr = regionPtr->borderListPtr; /* private */
    list_iter_t it;
    long numDelta = 0L;
    assert(edgeMapPtr);

    //[wer210] don't return a struct
    //    coordinate_t centerCoordinate = element_getNewPoint(elementPtr);
    coordinate_t centerCoordinate;
    element_getNewPoint(elementPtr, &centerCoordinate);
    /*
     * Remove the old triangles
     */

    it = &(beforeListPtr->head);

    while (it ->nextPtr != NULL) {
      element_t* beforeElementPtr = (element_t*)it->nextPtr->dataPtr;
      it = it->nextPtr;

      TMMESH_REMOVE(meshPtr, beforeElementPtr);
    }


    numDelta -= TMLIST_GETSIZE(beforeListPtr);

    /*
     * If segment is encroached, split it in half
     */

    if (element_getNumEdge(elementPtr) == 1) {

        coordinate_t coordinates[2];

        edge_t* edgePtr = element_getEdge(elementPtr, 0);
        coordinates[0] = centerCoordinate;

        coordinates[1] = *(coordinate_t*)(edgePtr->firstPtr);
        element_t* aElementPtr = TMELEMENT_ALLOC(coordinates, 2);
        assert(aElementPtr);
        TMMESH_INSERT(meshPtr, aElementPtr, edgeMapPtr);

        coordinates[1] = *(coordinate_t*)(edgePtr->secondPtr);
        element_t* bElementPtr = TMELEMENT_ALLOC(coordinates, 2);
        assert(bElementPtr);
        TMMESH_INSERT(meshPtr, bElementPtr, edgeMapPtr);

        bool_t status;
        status = TMMESH_REMOVEBOUNDARY(meshPtr, element_getEdge(elementPtr, 0));
        assert(status);
        status = TMMESH_INSERTBOUNDARY(meshPtr, element_getEdge(aElementPtr, 0));
        assert(status);
        status = TMMESH_INSERTBOUNDARY(meshPtr, element_getEdge(bElementPtr, 0));
        assert(status);

        numDelta += 2;
    }

    /*
     * Insert the new triangles. These are contructed using the new
     * point and the two points from the border segment.
     */
    it = &(borderListPtr->head);
    //list_iter_reset(&it, borderListPtr);

    while (it->nextPtr != NULL) {
   //while (list_iter_hasNext(&it, borderListPtr)) {
      element_t* afterElementPtr;
      coordinate_t coordinates[3];

      //edge_t* borderEdgePtr = (edge_t*)list_iter_next(&it, borderListPtr);
      edge_t* borderEdgePtr = (edge_t*) it->nextPtr->dataPtr;
      it = it->nextPtr;

      assert(borderEdgePtr);
      coordinates[0] = centerCoordinate;
      coordinates[1] = *(coordinate_t*)(borderEdgePtr->firstPtr);
      coordinates[2] = *(coordinate_t*)(borderEdgePtr->secondPtr);
      afterElementPtr = TMELEMENT_ALLOC(coordinates, 3);
      assert(afterElementPtr);
      TMMESH_INSERT(meshPtr, afterElementPtr, edgeMapPtr);
      if (element_isBad(afterElementPtr)) {
        TMaddToBadVector(  badVectorPtr, afterElementPtr);
      }
    }

    //numDelta += PLIST_GETSIZE(borderListPtr);
    numDelta += TMLIST_GETSIZE(borderListPtr);

    return numDelta;
}


/* =============================================================================
 * TMgrowRegion
 * -- Return NULL if success, else pointer to encroached boundary
 * =============================================================================
 */
TM_SAFE
element_t*
TMgrowRegion (element_t* centerElementPtr,
              region_t* regionPtr,
              MAP_T* edgeMapPtr,
              bool_t* success)
{
  *success = TRUE;
    bool_t isBoundary = FALSE;

    //TM_SAFE
    if (element_getNumEdge(centerElementPtr) == 1) {
        isBoundary = TRUE;
        //TMprints("enter here\n");
    }

    list_t* beforeListPtr = regionPtr->beforeListPtr;
    list_t* borderListPtr = regionPtr->borderListPtr;
    queue_t* expandQueuePtr = regionPtr->expandQueuePtr;

    list_clear(beforeListPtr);
    list_clear(borderListPtr);
    TMQUEUE_CLEAR(expandQueuePtr);

    //[wer210]
    //coordinate_t centerCoordinate = element_getNewPoint(centerElementPtr);
    coordinate_t centerCoordinate;
    element_getNewPoint(centerElementPtr, &centerCoordinate);

    coordinate_t* centerCoordinatePtr = &centerCoordinate;

    TMQUEUE_PUSH(expandQueuePtr, (void*)centerElementPtr);
    while (!TMQUEUE_ISEMPTY(expandQueuePtr)) {

        element_t* currentElementPtr = (element_t*)TMQUEUE_POP(expandQueuePtr);

        TMLIST_INSERT(beforeListPtr, (void*)currentElementPtr); /* no duplicates */
        // TM_SAFE
        list_t* neighborListPtr = element_getNeighborListPtr(currentElementPtr);

        list_iter_t it;
        it = &(neighborListPtr->head);

        while (it->nextPtr != NULL) {
          element_t* neighborElementPtr = (element_t*)it->nextPtr->dataPtr;
          it = it->nextPtr;

            TMELEMENT_ISGARBAGE(neighborElementPtr); /* so we can detect conflicts */
            if (!list_find(beforeListPtr, (void*)neighborElementPtr)) {
              //[wer210] below function includes acos() and sqrt(), now safe
              if (element_isInCircumCircle(neighborElementPtr, centerCoordinatePtr)) {
                  /* This is part of the region */
                  if (!isBoundary && (element_getNumEdge(neighborElementPtr) == 1)) {
                        /* Encroached on mesh boundary so split it and restart */
                        return neighborElementPtr;
                    } else {
                        /* Continue breadth-first search */
                        bool_t isSuccess;
                        isSuccess = TMQUEUE_PUSH(expandQueuePtr,(void*)neighborElementPtr);
                        assert(isSuccess);
                    }
                } else {
                    /* This element borders region; save info for retriangulation */
                edge_t* borderEdgePtr =
                        element_getCommonEdge(neighborElementPtr, currentElementPtr);
                    if (!borderEdgePtr) {
                      //_ITM_abortTransaction(2); // TM_restart
                      *success = FALSE;
                      return NULL;
                    }
                    TMLIST_INSERT(borderListPtr,(void*)borderEdgePtr); /* no duplicates */
                    if (!MAP_CONTAINS(edgeMapPtr, borderEdgePtr)) {
                        MAP_INSERT(edgeMapPtr, borderEdgePtr, neighborElementPtr);
                    }
                }
            } /* not visited before */

        } /* for each neighbor */

    } /* breadth-first search */

    return NULL;
}


/* =============================================================================
 * TMregion_refine
 * -- Returns net number of elements added to mesh
 * =============================================================================
 */
TM_SAFE
long
TMregion_refine (region_t* regionPtr, element_t* elementPtr, mesh_t* meshPtr, bool_t* success)
{

    long numDelta = 0L;
    MAP_T* edgeMapPtr = NULL;
    element_t* encroachElementPtr = NULL;

    if (TMELEMENT_ISGARBAGE(elementPtr))
      return numDelta; /* so we can detect conflicts */

    while (1) {
      //[wer] MAP_ALLOC = jsw_avlnew(cmp), where cmp should be SAFE
        edgeMapPtr = MAP_ALLOC(NULL, &element_mapCompareEdge);
        assert(edgeMapPtr);
        //[wer210] added one more parameter "success" to indicate successfulness
        encroachElementPtr = TMgrowRegion(elementPtr,
                                          regionPtr,
                                          edgeMapPtr,
                                          success);

        if (encroachElementPtr) {
            TMELEMENT_SETISREFERENCED(encroachElementPtr, TRUE);
            numDelta += TMregion_refine(regionPtr,
                                        encroachElementPtr,
                                        meshPtr,
                                        success);
            if (TMELEMENT_ISGARBAGE(elementPtr)) {
                break;
            }
        } else {
            break;
        }
        MAP_FREE(edgeMapPtr); // jsw_avldelete(edgeMapPtr)
    }

    /*
     * Perform retriangulation.
     */

    if (!TMELEMENT_ISGARBAGE(elementPtr)) {
      numDelta += TMretriangulate(elementPtr,
                                    regionPtr,
                                    meshPtr,
                                    edgeMapPtr);
    }

    MAP_FREE(edgeMapPtr); /* no need to free elements */

    return numDelta;
}


/* =============================================================================
 * Pregion_clearBad
 * =============================================================================
 */
//TM_PURE
TM_SAFE
void
Pregion_clearBad (region_t* regionPtr)
{
    PVECTOR_CLEAR(regionPtr->badVectorPtr);
}


/* =============================================================================
 * TMregion_transferBad
 * =============================================================================
 */
TM_SAFE
void
TMregion_transferBad (region_t* regionPtr, heap_t* workHeapPtr)
{
    vector_t* badVectorPtr = regionPtr->badVectorPtr;
    long numBad = PVECTOR_GETSIZE(badVectorPtr);
    long i;

    for (i = 0; i < numBad; i++) {
        element_t* badElementPtr = (element_t*)vector_at(badVectorPtr, i);
        if (TMELEMENT_ISGARBAGE(badElementPtr)) {
            TMELEMENT_FREE(badElementPtr);
        } else {
            bool_t status = TMHEAP_INSERT(workHeapPtr, (void*)badElementPtr);
            assert(status);
        }
    }
}


/* =============================================================================
 *
 * End of region.c
 *
 * =============================================================================
 */
