/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "coordinate.h"
#include "element.h"
#include "tm_transition.h"
#include "tm_hacks.h"

#if defined(TEST_ELEMENT) || defined(TEST_MESH)
double global_angleConstraint = 20.0;
#else
extern double global_angleConstraint;
#endif


/* =============================================================================
 * minimizeCoordinates
 * -- put smallest coordinate in position 0
 * =============================================================================
 */
 __attribute__((transaction_safe))
void minimizeCoordinates(element_t* elementPtr)
{
    coordinate_t* coordinates = elementPtr->coordinates;
    long numCoordinate = elementPtr->numCoordinate;
    long minPosition = 0;

    for (long i = 1; i < numCoordinate; i++) {
        if (coordinate_compare(&coordinates[i], &coordinates[minPosition]) < 0) {
            minPosition = i;
        }
    }

    while (minPosition != 0) {
        coordinate_t tmp = coordinates[0];
        long j;
        for (j = 0; j < (numCoordinate - 1); j++) {
            coordinates[j] = coordinates[j+1];
        }
        coordinates[numCoordinate-1] = tmp;
        minPosition--;
    }
}


/* =============================================================================
 * checkAngles
 * -- Sets isSkinny to true if the angle constraint is not met
 * =============================================================================
 */
//[wer210]This should be __attribute__((transaction_safe)), as it is called inside a TM_safe function.
 __attribute__((transaction_safe))
void
checkAngles (element_t* elementPtr)
{
    long numCoordinate = elementPtr->numCoordinate;
    double angleConstraint = global_angleConstraint;
    double minAngle = 180.0;

    assert(numCoordinate == 2 || numCoordinate == 3);
    elementPtr->isReferenced = false;
    elementPtr->isSkinny = false;
    elementPtr->encroachedEdgePtr = NULL;

    if (numCoordinate == 3) {
        long i;
        coordinate_t* coordinates = elementPtr->coordinates;
        for (i = 0; i < 3; i++) {
            double angle = coordinates[i].angle(&coordinates[(i + 1) % 3],
                                                &coordinates[(i + 2) % 3]);
            assert(angle > 0.0);
            assert(angle < 180.0);
            if (angle > 90.0) {
                elementPtr->encroachedEdgePtr = &elementPtr->edges[(i + 1) % 3];
            }
            if (angle < angleConstraint) {
                elementPtr->isSkinny = true;
            }
            if (angle < minAngle) {
                minAngle = angle;
            }
        }
        assert(minAngle < 180.0);
    }

    elementPtr->minAngle = minAngle;
}


/* =============================================================================
 * calculateCircumCenter
 *
 * Given three points A(ax,ay), B(bx,by), C(cx,cy), circumcenter R(rx,ry):
 *
 *              |                         |
 *              | by - ay   (||b - a||)^2 |
 *              |                         |
 *              | cy - ay   (||c - a||)^2 |
 *              |                         |
 *   rx = ax - -----------------------------
 *                   |                   |
 *                   | bx - ax   by - ay |
 *               2 * |                   |
 *                   | cx - ax   cy - ay |
 *                   |                   |
 *
 *              |                         |
 *              | bx - ax   (||b - a||)^2 |
 *              |                         |
 *              | cx - ax   (||c - a||)^2 |
 *              |                         |
 *   ry = ay + -----------------------------
 *                   |                   |
 *                   | bx - ax   by - ay |
 *               2 * |                   |
 *                   | cx - ax   cy - ay |
 *                   |                   |
 *
 * =============================================================================
 */
//[wer210] should be __attribute__((transaction_safe))
 __attribute__((transaction_safe))
void
calculateCircumCircle (element_t* elementPtr)
{
    long numCoordinate = elementPtr->numCoordinate;
    coordinate_t* coordinates = elementPtr->coordinates;
    coordinate_t* circumCenterPtr = &elementPtr->circumCenter;

    assert(numCoordinate == 2 || numCoordinate == 3);

    if (numCoordinate == 2) {
        circumCenterPtr->x = (coordinates[0].x + coordinates[1].x) / 2.0;
        circumCenterPtr->y = (coordinates[0].y + coordinates[1].y) / 2.0;
    } else {
        double ax = coordinates[0].x;
        double ay = coordinates[0].y;
        double bx = coordinates[1].x;
        double by = coordinates[1].y;
        double cx = coordinates[2].x;
        double cy = coordinates[2].y;
        double bxDelta = bx - ax;
        double byDelta = by - ay;
        double cxDelta = cx - ax;
        double cyDelta = cy - ay;
        double bDistance2 = (bxDelta * bxDelta) + (byDelta * byDelta);
        double cDistance2 = (cxDelta * cxDelta) + (cyDelta * cyDelta);
        double xNumerator = (byDelta * cDistance2) - (cyDelta * bDistance2);
        double yNumerator = (bxDelta * cDistance2) - (cxDelta * bDistance2);
        double denominator = 2 * ((bxDelta * cyDelta) - (cxDelta * byDelta));
        double rx = ax - (xNumerator / denominator);
        double ry = ay + (yNumerator / denominator);
        assert(fabs(denominator) > DBL_MIN); /* make sure not colinear */
        circumCenterPtr->x = rx;
        circumCenterPtr->y = ry;
    }

    elementPtr->circumRadius = circumCenterPtr->distance(&coordinates[0]);
}


/* =============================================================================
 * setEdge
 *
  * Note: Makes pairPtr sorted; i.e., coordinate_compare(first, second) < 0
 * =============================================================================
 */
//static
__attribute__((transaction_safe))
void
setEdge (element_t* elementPtr, long i)
{
    long numCoordinate = elementPtr->numCoordinate;
    coordinate_t* coordinates = elementPtr->coordinates;

    coordinate_t* firstPtr = &coordinates[i];
    coordinate_t* secondPtr = &coordinates[(i + 1) % numCoordinate];

    edge_t* edgePtr = &elementPtr->edges[i];

    long cmp = coordinate_compare(firstPtr, secondPtr);
    assert(cmp != 0);
    if (cmp < 0) {
        edgePtr->first  = firstPtr;
        edgePtr->second = secondPtr;
    } else {
        edgePtr->first  = secondPtr;
        edgePtr->second = firstPtr;
    }

    coordinate_t* midpointPtr = &elementPtr->midpoints[i];
    midpointPtr->x = (firstPtr->x + secondPtr->x) / 2.0;
    midpointPtr->y = (firstPtr->y + secondPtr->y) / 2.0;

    elementPtr->radii[i] = firstPtr->distance(midpointPtr);
}


/* =============================================================================
 * initEdges
 * =============================================================================
 */
__attribute__((transaction_safe))
void
initEdges (element_t* elementPtr, long numCoordinate)
{
    long numEdge = ((numCoordinate * (numCoordinate - 1)) / 2);

    elementPtr->numEdge = numEdge;

    long e;
    for (e = 0; e < numEdge; e++) {
        setEdge(elementPtr, e);
    }
}


/* =============================================================================
 * element_compare
 * =============================================================================
 */
__attribute__((transaction_safe))
long
element_compare (const element_t* aElementPtr, const element_t* bElementPtr)
{
    long aNumCoordinate = aElementPtr->numCoordinate;
    long bNumCoordinate = bElementPtr->numCoordinate;
    const coordinate_t* aCoordinates = aElementPtr->coordinates;
    const coordinate_t* bCoordinates = bElementPtr->coordinates;

    if (aNumCoordinate < bNumCoordinate) {
        return -1;
    } else if (aNumCoordinate > bNumCoordinate) {
        return 1;
    }

    long i;
    for (i = 0; i < aNumCoordinate; i++) {
        long compareCoordinate =
            coordinate_compare(&aCoordinates[i], &bCoordinates[i]);
        if (compareCoordinate != 0) {
            return compareCoordinate;
        }
    }

    return 0;
}


/* =============================================================================
 * element_listCompare
 *
 * For use in list_t
 * =============================================================================
 */
__attribute__((transaction_safe))
long
element_listCompare (const void* aPtr, const void* bPtr)
{
    element_t* aElementPtr = (element_t*)aPtr;
    element_t* bElementPtr = (element_t*)bPtr;

    return element_compare(aElementPtr, bElementPtr);
}

__attribute__((transaction_safe))
element_t::element_t(coordinate_t* _coordinates, long _numCoordinate)
{
    for (long i = 0; i < _numCoordinate; i++) {
        coordinates[i] = _coordinates[i];
    }
    numCoordinate = _numCoordinate;
    minimizeCoordinates(this);
    checkAngles(this);
    calculateCircumCircle(this);
    initEdges(this, numCoordinate);
    neighborListPtr = new std::set<element_t*, element_listCompare_t>();
    assert(neighborListPtr);
    isGarbage = false;
    isReferenced = false;
}

__attribute__((transaction_safe))
element_t::~element_t()
{
    delete neighborListPtr;
}

__attribute__((transaction_safe))
long element_t::getNumEdge ()
{
    return numEdge;
}


/* =============================================================================
 * element_getEdge
 *
 * Returned edgePtr is sorted; i.e., coordinate_compare(first, second) < 0
 * =============================================================================
 */
__attribute__((transaction_safe))
edge_t* element_t::getEdge(long i)
{
    if (i < 0 || i > numEdge) {
        return NULL;
    }

    return &edges[i];
}


/* =============================================================================
 * element_compareEdge
 * =============================================================================
 */
 __attribute__((transaction_safe))
long
compareEdge (const edge_t* aEdgePtr, const edge_t* bEdgePtr)
{
    long diffFirst = coordinate_compare(aEdgePtr->first, bEdgePtr->first);

    return ((diffFirst != 0) ?
            (diffFirst) :
            (coordinate_compare(aEdgePtr->second, bEdgePtr->second)));
}


/* ============================================================================
 * element_listCompareEdge
 *
 * For use in list_t
 * ============================================================================
 */
__attribute__((transaction_safe))
long
element_listCompareEdge (const void* aPtr, const void* bPtr)
{
    edge_t* aEdgePtr = (edge_t*)(aPtr);
    edge_t* bEdgePtr = (edge_t*)(bPtr);

    return compareEdge(aEdgePtr, bEdgePtr);
}

bool element_heapCompare_t::operator()(const element_t* a, const element_t* b)
{
   if (a->encroachedEdgePtr) {
        if (b->encroachedEdgePtr) {
            return false; /* do not care */
        } else {
            return false; // gt, so flip
        }
    }

    if (b->encroachedEdgePtr) {
        return true; // lt, so don't flip
    }

    return false; /* do not care */
}

__attribute__((transaction_safe))
bool element_t::isInCircumCircle(coordinate_t* coordinatePtr)
{
    double distance = coordinatePtr->distance(&circumCenter);
    return ((distance <= circumRadius) ? true : false);
}


/* =============================================================================
 * isEncroached
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
isEncroached (element_t* elementPtr)
{
    return ((elementPtr->encroachedEdgePtr != NULL) ? true : false);
}


__attribute__((transaction_safe))
void element_t::clearEncroached ()
{
    encroachedEdgePtr = NULL;
}

__attribute__((transaction_safe))
edge_t* element_t::getEncroachedPtr()
{
    return encroachedEdgePtr;
}

__attribute__((transaction_safe))
bool element_t::isEltSkinny()
{
    return ((isSkinny) ? true : false);
}


/* =============================================================================
 * element_isBad
 * -- Does it need to be refined?
 * =============================================================================
 */
__attribute__((transaction_safe))
bool element_t::isBad()
{
    return ((isEncroached(this) || isEltSkinny()) ? true : false);
}

__attribute__((transaction_safe))
void element_t::setIsReferenced(bool status)
{
    isReferenced = status;
}

/* =============================================================================
 * TMelement_isGarbage
 * -- Can we deallocate?
 * =============================================================================
 */
__attribute__((transaction_safe))
bool element_t::isEltGarbage()
{
    return isGarbage;
}

__attribute__((transaction_safe))
void element_t::setIsGarbage(bool status)
{
    isGarbage = status;
}

__attribute__((transaction_safe))
void element_t::addNeighbor(element_t* neighborPtr)
{
    custom_set_insertion(neighborListPtr, neighborPtr);
}

__attribute__((transaction_safe))
std::set<element_t*, element_listCompare_t>* element_t::getNeighborListPtr()
{
    return neighborListPtr;
}

/* =============================================================================
 * element_getCommonEdge
 *
 * Returns pointer to aElementPtr's shared edge
 * =============================================================================
 */
__attribute__((transaction_safe))
edge_t*
element_getCommonEdge (element_t* aElementPtr, element_t* bElementPtr)
{
    edge_t* aEdges = aElementPtr->edges;
    edge_t* bEdges = bElementPtr->edges;
    long aNumEdge = aElementPtr->numEdge;
    long bNumEdge = bElementPtr->numEdge;
    long a;
    long b;

    for (a = 0; a < aNumEdge; a++) {
        edge_t* aEdgePtr = &aEdges[a];
        for (b = 0; b < bNumEdge; b++) {
            edge_t* bEdgePtr = &bEdges[b];
            if (compareEdge(aEdgePtr, bEdgePtr) == 0) {
                return aEdgePtr;
            }
        }
    }

    return NULL;
}


/* =============================================================================
 * element_getNewPoint
 * -- Either the element is encroached or is skinny, so get the new point to add
 * =============================================================================
 */
__attribute__((transaction_safe))
//coordinate_t
void element_t::getNewPoint(coordinate_t* ret)
{
    if (encroachedEdgePtr) {
        for (long e = 0; e < numEdge; e++) {
            if (compareEdge(encroachedEdgePtr, &edges[e]) == 0) {
              (*ret).x = midpoints[e].x;
              (*ret).y = midpoints[e].y;
              //return elementPtr->midpoints[e];
              return;
            }
        }
        assert(0);
    }

    (*ret).x = circumCenter.x;
    (*ret).y = circumCenter.y;
    //  return elementPtr->circumCenter;
}


/* =============================================================================
 * element_checkAngles
 *
 * Return false if minimum angle constraint not met
 * =============================================================================
 */
bool element_t::eltCheckAngles ()
{
    double angleConstraint = global_angleConstraint;

    if (numCoordinate == 3) {
        for (long i = 0; i < 3; i++) {
            double angle = coordinates[i].angle(&coordinates[(i + 1) % 3],
                                                &coordinates[(i + 2) % 3]);
            if (angle < angleConstraint) {
                return false;
            }
        }
    }

    return true;
}


/* =============================================================================
 * element_print
 * =============================================================================
 */
void element_t::print()
{
    for (long c = 0; c < numCoordinate; c++) {
        coordinates[c].print();
        printf(" ");
    }
}


/* =============================================================================
 * element_printEdge
 * =============================================================================
 */
void element_printEdge (edge_t* edgePtr)
{
    edgePtr->first->print();
    printf(" -> ");
    edgePtr->second->print();
}


/* =============================================================================
 * element_printAngles
 * =============================================================================
 */
void element_t::printAngles()
{
    if (numCoordinate == 3) {
        for (long i = 0; i < 3; i++) {
            double angle = coordinates[i].angle(&coordinates[(i + 1) % 3],
                                                &coordinates[(i + 2) % 3]);
            printf("%0.3lf ", angle);
        }
    }
}

bool element_mapCompareEdge_t::operator()(const edge_t* left, const edge_t* right)
{
    return compareEdge(left, right) < 0;
}

bool element_listCompare_t::operator()(const element_t* aPtr, const element_t* bPtr)
{
    return element_listCompare(aPtr, bPtr) < 0;
}

#ifdef TEST_ELEMENT
/* =============================================================================
 * TEST_ELEMENT
 * =============================================================================
 */


#include <assert.h>
#include <stdio.h>


static void
printElement (element_t* elementPtr)
{
    long numCoordinate = elementPtr->numCoordinate;
    coordinate_t* coordinates = elementPtr->coordinates;
    long i;

    printf("%li: ", elementPtr->numCoordinate);
    for (i = 0; i < numCoordinate; i++) {
        printf("(%.2lf, %.2lf) ", coordinates[i].x, coordinates[i].y);
    }
    printf("| (%.2lf, %.2lf)",
            elementPtr->circumCenter.x, elementPtr->circumCenter.y);
    printf(" | isBad = %li", (long)element_isBad(elementPtr));
    puts("");
}


int
main (int argc, char* argv[])
{
    element_t* elementPtr;
    coordinate_t coordinates[4];

    coordinates[0].x = 1;
    coordinates[0].y = 0;
    coordinates[1].x = 0;
    coordinates[1].y = 1;
    coordinates[2].x = 0;
    coordinates[2].y = 0;
    coordinates[3].x = -2;
    coordinates[3].y = -2;

    elementPtr = element_alloc(coordinates, 3);
    assert(elementPtr);
    printElement(elementPtr);
    element_free(elementPtr);

    elementPtr = element_alloc(coordinates, 2);
    assert(elementPtr);
    printElement(elementPtr);
    element_free(elementPtr);

    elementPtr = element_alloc(coordinates+1, 3);
    assert(elementPtr);
    printElement(elementPtr);
    element_free(elementPtr);

    return 0;
}


#endif /* TEST_ELEMENT */
