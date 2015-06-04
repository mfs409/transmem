/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <stdlib.h>
#include "region.h"
#include "element.h"
#include "mesh.h"
#include "tm_transition.h"
#include "tm_hacks.h"

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */

__attribute__((transaction_safe))
void
TMaddToBadVector(std::vector<element_t*>* badVectorPtr, element_t* badElementPtr);

__attribute__((transaction_safe))
long
TMretriangulate (element_t* elementPtr,
                 region_t* regionPtr,
                 mesh_t* meshPtr,
                 std::map<edge_t*, element_t*, element_mapCompareEdge_t>* edgeMapPtr);

__attribute__((transaction_safe))
element_t*
TMgrowRegion (element_t* centerElementPtr,
              region_t* regionPtr,
              std::map<edge_t*, element_t*, element_mapCompareEdge_t>* edgeMapPtr,
              bool* success);

region_t::region_t()
{
    expandQueuePtr = new std::queue<element_t*>();
    assert(expandQueuePtr);

    //[wer210] note the following compare functions should be TM_SAFE...
    beforeListPtr = new std::set<element_t*, element_listCompare_t>();
    assert(beforeListPtr);

    borderListPtr = new std::set<edge_t*, element_listCompareEdge_t>();
    assert(borderListPtr);

    badVectorPtr = new std::vector<element_t*>();
    assert(badVectorPtr);
}

region_t::~region_t()
{
    delete badVectorPtr;
    delete borderListPtr;
    delete beforeListPtr;
    delete expandQueuePtr;
}

__attribute__((transaction_safe))
void
TMaddToBadVector (std::vector<element_t*>* badVectorPtr, element_t* badElementPtr)
{
    badVectorPtr->push_back(badElementPtr);
    badElementPtr->setIsReferenced(true);
}


/* =============================================================================
 * TMretriangulate
 * -- Returns net amount of elements added to mesh
 * =============================================================================
 */

__attribute__((transaction_safe))
long
TMretriangulate (element_t* elementPtr,
                 region_t* regionPtr,
                 mesh_t* meshPtr,
                 std::map<edge_t*, element_t*, element_mapCompareEdge_t>* edgeMapPtr)
{
    std::vector<element_t*>* badVectorPtr = regionPtr->badVectorPtr; /* private */
    auto beforeListPtr = regionPtr->beforeListPtr; /* private */
    auto borderListPtr = regionPtr->borderListPtr; /* private */
    long numDelta = 0L;
    assert(edgeMapPtr);

    //[wer210] don't return a struct
    //    coordinate_t centerCoordinate = element_getNewPoint(elementPtr);
    coordinate_t centerCoordinate;
    elementPtr->getNewPoint(&centerCoordinate);

    /*
     * Remove the old triangles
     */
    for (auto iter = beforeListPtr->begin(); iter != beforeListPtr->end(); ++iter) {
        element_t* elt = *iter;
        meshPtr->remove(elt);
    }

    numDelta -= beforeListPtr->size();

    /*
     * If segment is encroached, split it in half
     */

    if (elementPtr->getNumEdge() == 1) {

        coordinate_t coordinates[2];

        edge_t* edgePtr = elementPtr->getEdge(0);
        coordinates[0] = centerCoordinate;

        coordinates[1] = *edgePtr->first;
        element_t* aElementPtr = new element_t(coordinates, 2);
        assert(aElementPtr);
        meshPtr->insert(aElementPtr, edgeMapPtr);

        coordinates[1] = *edgePtr->second;
        element_t* bElementPtr = new element_t(coordinates, 2);
        assert(bElementPtr);
        meshPtr->insert(bElementPtr, edgeMapPtr);

        bool status;
        status = meshPtr->removeBoundary(elementPtr->getEdge(0));
        assert(status);
        status = meshPtr->insertBoundary(aElementPtr->getEdge(0));
        assert(status);
        status = meshPtr->insertBoundary(bElementPtr->getEdge(0));
        assert(status);

        numDelta += 2;
    }

    /*
     * Insert the new triangles. These are contructed using the new
     * point and the two points from the border segment.
     */
    for (auto iter = borderListPtr->begin(); iter != borderListPtr->end(); ++iter) {
        //while (list_iter_hasNext(&it, borderListPtr)) {
        element_t* afterElementPtr;
        coordinate_t coordinates[3];

        //edge_t* borderEdgePtr = (edge_t*)list_iter_next(&it, borderListPtr);
        edge_t* borderEdgePtr = *iter;
        assert(borderEdgePtr);
        coordinates[0] = centerCoordinate;
        coordinates[1] = *borderEdgePtr->first;
        coordinates[2] = *borderEdgePtr->second;
        afterElementPtr = new element_t(coordinates, 3);
        assert(afterElementPtr);
        meshPtr->insert(afterElementPtr, edgeMapPtr);
        if (afterElementPtr->isBad()) {
            TMaddToBadVector(badVectorPtr, afterElementPtr);
        }
    }

    //numDelta += PLIST_GETSIZE(borderListPtr);
    numDelta += borderListPtr->size();

    return numDelta;
}


/* =============================================================================
 * TMgrowRegion
 * -- Return NULL if success, else pointer to encroached boundary
 * =============================================================================
 */
__attribute__((transaction_safe))
element_t*
TMgrowRegion (element_t* centerElementPtr,
              region_t* regionPtr,
              std::map<edge_t*, element_t*, element_mapCompareEdge_t>* edgeMapPtr,
              bool* success)
{
    *success = true;
    bool isBoundary = false;

    //TM_SAFE
    if (centerElementPtr->getNumEdge() == 1) {
        isBoundary = true;
    }

    auto beforeListPtr = regionPtr->beforeListPtr;
    auto borderListPtr = regionPtr->borderListPtr;
    std::queue<element_t*>* expandQueuePtr = regionPtr->expandQueuePtr;

    beforeListPtr->clear();
    borderListPtr->clear();
    while (!expandQueuePtr->empty())
        expandQueuePtr->pop();

    //[wer210]
    //coordinate_t centerCoordinate = element_getNewPoint(centerElementPtr);
    coordinate_t centerCoordinate;
    centerElementPtr->getNewPoint(&centerCoordinate);

    coordinate_t* centerCoordinatePtr = &centerCoordinate;

    expandQueuePtr->push(centerElementPtr);
    while (!expandQueuePtr->empty()) {

        element_t* currentElementPtr = expandQueuePtr->front();
        expandQueuePtr->pop();

        custom_set_insertion(beforeListPtr, currentElementPtr); /* no duplicates */
        // __attribute__((transaction_safe))
        auto neighborListPtr = currentElementPtr->getNeighborListPtr();

        for (auto it : *neighborListPtr) {
            element_t* neighborElementPtr = it;

            // [TODO] This is extremely bad programming.  We shouldn't need this!
            neighborElementPtr->isEltGarbage(); /* so we can detect conflicts */
            if (beforeListPtr->find(neighborElementPtr) == beforeListPtr->end()) {
              //[wer210] below function includes acos() and sqrt(), now safe
              if (neighborElementPtr->isInCircumCircle(centerCoordinatePtr)) {
                  /* This is part of the region */
                  if (!isBoundary && (neighborElementPtr->getNumEdge() == 1)) {
                        /* Encroached on mesh boundary so split it and restart */
                        return neighborElementPtr;
                    } else {
                        /* Continue breadth-first search */
                        expandQueuePtr->push(neighborElementPtr);
                    }
                } else {
                    /* This element borders region; save info for retriangulation */
                    edge_t* borderEdgePtr =
                        element_getCommonEdge(neighborElementPtr, currentElementPtr);
                    if (!borderEdgePtr) {
                      //_ITM_abortTransaction(2); // TM_restart
                      *success = false;
                      return NULL;
                    }
                    custom_set_insertion(borderListPtr, borderEdgePtr); /* no duplicates */
                    if (edgeMapPtr->find(borderEdgePtr) == edgeMapPtr->end()) {
                        custom_map_insertion(edgeMapPtr, borderEdgePtr, neighborElementPtr);
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
__attribute__((transaction_safe))
long region_t::refine(element_t* elementPtr, mesh_t* meshPtr, bool* success)
{
    long numDelta = 0L;
    std::map<edge_t*, element_t*, element_mapCompareEdge_t>* edgeMapPtr = NULL;
    element_t* encroachElementPtr = NULL;

    if (elementPtr->isEltGarbage())
      return numDelta; /* so we can detect conflicts */

    while (1) {
      //[wer] MAP_ALLOC = jsw_avlnew(cmp), where cmp should be SAFE
        edgeMapPtr = new std::map<edge_t*, element_t*, element_mapCompareEdge_t>();
        assert(edgeMapPtr);
        //[wer210] added one more parameter "success" to indicate successfulness
        encroachElementPtr = TMgrowRegion(elementPtr,
                                          this,
                                          edgeMapPtr,
                                          success);

        if (encroachElementPtr) {
            encroachElementPtr->setIsReferenced(true);
            numDelta += refine(encroachElementPtr, meshPtr, success);
            if (elementPtr->isEltGarbage()) {
                break;
            }
        } else {
            break;
        }
        delete edgeMapPtr; // jsw_avldelete(edgeMapPtr)
    }

    /*
     * Perform retriangulation.
     */

    if (!elementPtr->isEltGarbage()) {
      numDelta += TMretriangulate(elementPtr, this, meshPtr, edgeMapPtr);
    }

    delete edgeMapPtr; /* no need to free elements */

    return numDelta;
}


__attribute__((transaction_safe))
void region_t::clearBad()
{
    badVectorPtr->clear();
}

__attribute__((transaction_safe))
void region_t::transferBad(std::multiset<element_t*, element_heapCompare_t>* workHeap)
{
    long numBad = badVectorPtr->size();

    for (long i = 0; i < numBad; i++) {
        element_t* badElementPtr = badVectorPtr->at(i);
        if (badElementPtr->isEltGarbage()) {
            delete badElementPtr;
        } else {
            bool status = custom_set_insertion(workHeap, badElementPtr);
            assert(status);
        }
    }
}
