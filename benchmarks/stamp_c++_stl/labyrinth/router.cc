/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <stdlib.h>
#include "coordinate.h"
#include "grid.h"
#include "router.h"
#include <vector>
#include "tm_transition.h"

enum momentum_t {
    MOMENTUM_ZERO = 0,
    MOMENTUM_POSX = 1,
    MOMENTUM_POSY = 2,
    MOMENTUM_POSZ = 3,
    MOMENTUM_NEGX = 4,
    MOMENTUM_NEGY = 5,
    MOMENTUM_NEGZ = 6
};

struct point_t {
    long x;
    long y;
    long z;
    long value;
    momentum_t momentum;
};

point_t MOVE_POSX = { 1,  0,  0,  0, MOMENTUM_POSX};
point_t MOVE_POSY = { 0,  1,  0,  0, MOMENTUM_POSY};
point_t MOVE_POSZ = { 0,  0,  1,  0, MOMENTUM_POSZ};
point_t MOVE_NEGX = {-1,  0,  0,  0, MOMENTUM_NEGX};
point_t MOVE_NEGY = { 0, -1,  0,  0, MOMENTUM_NEGY};
point_t MOVE_NEGZ = { 0,  0, -1,  0, MOMENTUM_NEGZ};


/* =============================================================================
 * router_alloc
 * =============================================================================
 */
router_t::router_t(long _xCost, long _yCost, long _zCost, long _bendCost)
{
    xCost = _xCost;
    yCost = _yCost;
    zCost = _zCost;
    bendCost = _bendCost;
}

/* =============================================================================
 * PexpandToNeighbor
 * =============================================================================
 */
//[wer210] was static
__attribute__((transaction_safe))
void
PexpandToNeighbor(grid_t* myGridPtr, long x, long y, long z, long value,
                  std::queue<long*>* queuePtr)
{
    if (myGridPtr->isPointValid(x, y, z)) {
        long* neighborGridPointPtr = myGridPtr->getPointRef(x, y, z);
        long neighborValue = *neighborGridPointPtr;
        if (neighborValue == GRID_POINT_EMPTY) {
            (*neighborGridPointPtr) = value;
            queuePtr->push(neighborGridPointPtr);
        } else if (neighborValue != GRID_POINT_FULL) {
            /* We have expanded here before... is this new path better? */
            if (value < neighborValue) {
                (*neighborGridPointPtr) = value;
                queuePtr->push(neighborGridPointPtr);
            }
        }
    }
}


/* =============================================================================
 * PdoExpansion
 * =============================================================================
 */
//static TM_PURE
__attribute__((transaction_safe))
bool
PdoExpansion (router_t* routerPtr, grid_t* myGridPtr, std::queue<long*>* queuePtr,
              coordinate_t* srcPtr, coordinate_t* dstPtr)
{
    long xCost = routerPtr->xCost;
    long yCost = routerPtr->yCost;
    long zCost = routerPtr->zCost;

    /*
     * Potential Optimization: Make 'src' the one closest to edge.
     * This will likely decrease the area of the emitted wave.
     */
    while (!queuePtr->empty())
        queuePtr->pop();

    // __attribute__((transaction_safe))
    long* srcGridPointPtr =
        myGridPtr->getPointRef(srcPtr->x, srcPtr->y, srcPtr->z);
    queuePtr->push(srcGridPointPtr);
    // __attribute__((transaction_safe))
    myGridPtr->setPoint(srcPtr->x, srcPtr->y, srcPtr->z, 0);
    myGridPtr->setPoint(dstPtr->x, dstPtr->y, dstPtr->z, GRID_POINT_EMPTY);
    long* dstGridPointPtr =
        myGridPtr->getPointRef(dstPtr->x, dstPtr->y, dstPtr->z);
    bool isPathFound = false;

    while (!queuePtr->empty()) {

        long* gridPointPtr = queuePtr->front(); queuePtr->pop();
        if (gridPointPtr == dstGridPointPtr) {
            isPathFound = true;
            break;
        }

        long x;
        long y;
        long z;
        // __attribute__((transaction_safe))
        myGridPtr->getPointIndices(gridPointPtr, &x, &y, &z);
        long value = (*gridPointPtr);

        /*
         * Check 6 neighbors
         *
         * Potential Optimization: Only need to check 5 of these
         */
        PexpandToNeighbor(myGridPtr, x+1, y,   z,   (value + xCost), queuePtr);
        PexpandToNeighbor(myGridPtr, x-1, y,   z,   (value + xCost), queuePtr);
        PexpandToNeighbor(myGridPtr, x,   y+1, z,   (value + yCost), queuePtr);
        PexpandToNeighbor(myGridPtr, x,   y-1, z,   (value + yCost), queuePtr);
        PexpandToNeighbor(myGridPtr, x,   y,   z+1, (value + zCost), queuePtr);
        PexpandToNeighbor(myGridPtr, x,   y,   z-1, (value + zCost), queuePtr);

    } /* iterate over work queue */

#ifdef DEBUG
    printf("Expansion (%li, %li, %li) -> (%li, %li, %li):\n",
           srcPtr->x, srcPtr->y, srcPtr->z,
           dstPtr->x, dstPtr->y, dstPtr->z);
    grid_print(myGridPtr);
#endif /*  DEBUG */

    return isPathFound;
}


/* =============================================================================
 * traceToNeighbor
 * =============================================================================
 */
__attribute__((transaction_safe))
//static
void
traceToNeighbor (grid_t* myGridPtr,
                 point_t* currPtr,
                 point_t* movePtr,
                 bool useMomentum,
                 long bendCost,
                 point_t* nextPtr)
{
    long x = currPtr->x + movePtr->x;
    long y = currPtr->y + movePtr->y;
    long z = currPtr->z + movePtr->z;

    if (myGridPtr->isPointValid(x, y, z) &&
        !myGridPtr->isPointEmpty(x, y, z) &&
        !myGridPtr->isPointFull(x, y, z))
    {
        long value = myGridPtr->getPoint(x, y, z);
        long b = 0;
        if (useMomentum && (currPtr->momentum != movePtr->momentum)) {
            b = bendCost;
        }
        if ((value + b) <= nextPtr->value) { /* '=' favors neighbors over current */
            nextPtr->x = x;
            nextPtr->y = y;
            nextPtr->z = z;
            nextPtr->value = value;
            nextPtr->momentum = movePtr->momentum;
        }
    }
}


/* =============================================================================
 * PdoTraceback
 * =============================================================================
 */
//static TM_PURE
__attribute__((transaction_safe))
std::vector<long*>*
PdoTraceback (grid_t* gridPtr, grid_t* myGridPtr,
              coordinate_t* dstPtr, long bendCost)
{
    std::vector<long*>* pointVectorPtr = new std::vector<long*>();
    assert(pointVectorPtr);

    point_t next;
    next.x = dstPtr->x;
    next.y = dstPtr->y;
    next.z = dstPtr->z;
    next.value = myGridPtr->getPoint(next.x, next.y, next.z);
    next.momentum = MOMENTUM_ZERO;

    while (1) {

        long* gridPointPtr = gridPtr->getPointRef(next.x, next.y, next.z);
        pointVectorPtr->push_back(gridPointPtr);
        myGridPtr->setPoint(next.x, next.y, next.z, GRID_POINT_FULL);

        /* Check if we are done */
        if (next.value == 0) {
            break;
        }
        point_t curr = next;

        /*
         * Check 6 neighbors
         *
         * Potential Optimization: Only need to check 5 of these
         */
        traceToNeighbor(myGridPtr, &curr, &MOVE_POSX, true, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_POSY, true, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_POSZ, true, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_NEGX, true, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_NEGY, true, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_NEGZ, true, bendCost, &next);

#ifdef DEBUG
        printf("(%li, %li, %li)\n", next.x, next.y, next.z);
#endif /* DEBUG */
        /*
         * Because of bend costs, none of the neighbors may appear to be closer.
         * In this case, pick a neighbor while ignoring momentum.
         */
        if ((curr.x == next.x) &&
            (curr.y == next.y) &&
            (curr.z == next.z))
        {
            next.value = curr.value;
            traceToNeighbor(myGridPtr, &curr, &MOVE_POSX, false, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_POSY, false, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_POSZ, false, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_NEGX, false, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_NEGY, false, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_NEGZ, false, bendCost, &next);

            if ((curr.x == next.x) &&
                (curr.y == next.y) &&
                (curr.z == next.z))
            {
                delete pointVectorPtr;
#ifdef DEBUG
                puts("[dead]");
#endif
                return NULL; /* cannot find path */
            }
        }
    }

#ifdef DEBUG
    puts("");
#endif /* DEBUG */

    return pointVectorPtr;
}


/* =============================================================================
 * router_solve
 * =============================================================================
 */
void router_solve(void* argPtr)
{
    router_solve_arg_t* routerArgPtr = (router_solve_arg_t*)argPtr;
    router_t* routerPtr = routerArgPtr->routerPtr;
    maze_t* mazePtr = routerArgPtr->mazePtr;
    std::vector<std::vector<long*>*>* myPathVectorPtr = new std::vector<std::vector<long*>*>();
    assert(myPathVectorPtr);

    std::queue<std::pair<coordinate_t*, coordinate_t*>*>* workQueuePtr = mazePtr->workQueuePtr;
    grid_t* gridPtr = mazePtr->gridPtr;
    grid_t* myGridPtr =
        new grid_t(gridPtr->width, gridPtr->height, gridPtr->depth);
    assert(myGridPtr);
    long bendCost = routerPtr->bendCost;
    std::queue<long*>* myExpansionQueuePtr = new std::queue<long*>();

    /*
     * Iterate over work list to route each path. This involves an
     * 'expansion' and 'traceback' phase for each source/destination pair.
     */
    while (1) {

        std::pair<coordinate_t*, coordinate_t*>* coordinatePairPtr;
        __transaction_atomic {
            if (workQueuePtr->empty()) {
                coordinatePairPtr = NULL;
          } else {
                coordinatePairPtr = workQueuePtr->front(); workQueuePtr->pop();
          }
        }
        if (coordinatePairPtr == NULL) {
            break;
        }

        coordinate_t* srcPtr = coordinatePairPtr->first;
        coordinate_t* dstPtr = coordinatePairPtr->second;

        delete coordinatePairPtr;

        bool success = false;
        std::vector<long*>* pointVectorPtr = NULL;

#if 0
        __transaction_atomic {
          grid_copy(myGridPtr, gridPtr); /* ok if not most up-to-date */
          if (PdoExpansion(routerPtr, myGridPtr, myExpansionQueuePtr,
                           srcPtr, dstPtr)) {
            pointVectorPtr = PdoTraceback(gridPtr, myGridPtr, dstPtr, bendCost);
            /*
             * TODO: fix memory leak
             *
             * pointVectorPtr will be a memory leak if we abort this transaction
             */
            if (pointVectorPtr) {
              // [wer210]__attribute__((transaction_safe)), abort inside
              TMGRID_ADDPATH(gridPtr, pointVectorPtr);
              TM_LOCAL_WRITE(success, true);
            }
          }
        }

#endif
        //[wer210] change the control flow
        while (true) {
          success = false;
          // get a snapshot of the grid... may be inconsistent, but that's OK
          grid_copy(myGridPtr, gridPtr);
          /* ok if not most up-to-date */
          // see if there is a valid path we can use
          if (PdoExpansion(routerPtr, myGridPtr, myExpansionQueuePtr, srcPtr, dstPtr)) {
            pointVectorPtr = PdoTraceback(gridPtr, myGridPtr, dstPtr, bendCost);

            if (pointVectorPtr) {
              // we've got a valid path.  Use a transaction to validate and finalize it
                bool validity = false;

                __transaction_atomic {
                  validity = gridPtr->TMaddPath(pointVectorPtr);
                }

              // if the operation was valid, we just finalized the path
              if (validity) {
                success = true;
                break;
              }

              // otherwise we need to resample the grid
              else {
                  // NB: doing things this way means we can fix a memory
                  // leak from the original STAMP labyrinth
                  delete pointVectorPtr;
                  continue;
              }
            }

            // if the traceback failed, we need to resample the grid
            else {
              continue;
            }
          }
          // if the traceback failed, then the current path is not possible, so
          // we should skip it
          else {
            break;
          }
        }
        //////// end of change
        if (success) {
            myPathVectorPtr->push_back(pointVectorPtr);
        }

    }

    /*
     * Add my paths to global list
     */
    __transaction_atomic {
        routerArgPtr->pathVectorListPtr->insert(myPathVectorPtr);
    }

    delete myGridPtr;
    delete myExpansionQueuePtr;

#ifdef DEBUG
    puts("\nFinal Grid:");
    grid_print(gridPtr);
#endif /* DEBUG */

}


/* =============================================================================
 *
 * End of router.c
 *
 * =============================================================================
 */
