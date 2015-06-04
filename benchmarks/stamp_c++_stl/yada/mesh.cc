/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include "element.h"
#include "mesh.h"
#include "utility.h"
#include "tm_transition.h"
#include "tm_hacks.h"

mesh_t::mesh_t()
{
    rootElementPtr = NULL;
    initBadQueuePtr = new std::queue<element_t*>();
    assert(initBadQueuePtr);
    size = 0;
    boundarySetPtr = new std::set<edge_t*, element_listCompareEdge_t>();
    assert(boundarySetPtr);
}

mesh_t::~mesh_t()
{
    delete initBadQueuePtr;
    delete boundarySetPtr;
}

__attribute__((transaction_safe))
void mesh_t::insert(element_t* elementPtr,
                    std::map<edge_t*, element_t*, element_mapCompareEdge_t>* edgeMapPtr)
{
    /*
     * Assuming fully connected graph, we just need to record one element.
     * The root element is not needed for the actual refining, but rather
     * for checking the validity of the final mesh.
     */
    if (!rootElementPtr) {
        rootElementPtr = elementPtr;
    }

    /*
     * Record existence of each of this element's edges
     */
    long i;
    long numEdge = elementPtr->getNumEdge();
    for (i = 0; i < numEdge; i++) {
        edge_t* edgePtr = elementPtr->getEdge(i);
        if (edgeMapPtr->find(edgePtr) == edgeMapPtr->end()) {
            /* Record existance of this edge */
            bool isSuccess;
            isSuccess = custom_map_insertion(edgeMapPtr, edgePtr, elementPtr);
            assert(isSuccess);
        } else {
            /*
             * Shared edge; update each element's neighborList
             */
            bool isSuccess;
            auto x = edgeMapPtr->find(edgePtr);
            element_t* sharerPtr = x->second;
            assert(sharerPtr); /* cannot be shared by >2 elements */
            elementPtr->addNeighbor(sharerPtr);
            sharerPtr->addNeighbor(elementPtr);
            isSuccess = edgeMapPtr->erase(edgePtr) == 1;
            assert(isSuccess);
            isSuccess = custom_map_insertion(edgeMapPtr, edgePtr, NULL); /* marker to check >2 sharers */
            assert(isSuccess);
        }
    }

    /*
     * Check if really encroached
     */

    edge_t* encroachedPtr = elementPtr->getEncroachedPtr();
    if (encroachedPtr) {
        if (boundarySetPtr->find(encroachedPtr) == boundarySetPtr->end()) {
            elementPtr->clearEncroached();
        }
    }
}

__attribute__((transaction_safe))
void mesh_t::remove(element_t* elementPtr)
{
    assert(!elementPtr->isEltGarbage());

    /*
     * If removing root, a new root is selected on the next mesh_insert, which
     * always follows a call a mesh_remove.
     */
    if (rootElementPtr == elementPtr) {
        rootElementPtr = NULL;
    }

    /*
     * Remove from neighbors
     */
    //list_t* neighborListPtr = element_getNeighborListPtr(elementPtr);
    auto neighborListPtr = elementPtr->neighborListPtr;
    for (auto iter : *neighborListPtr) {

        element_t * neighborPtr = iter;

      //list_t* neighborNeighborListPtr = element_getNeighborListPtr(neighborPtr);
      auto neighborNeighborListPtr = neighborPtr->neighborListPtr;
      bool status = neighborNeighborListPtr->erase(elementPtr) == 1;
      assert(status);
    }

    //TMELEMENT_SETISGARBAGE(elementPtr, true);
    elementPtr->isGarbage = true;

    //if (!TMELEMENT_ISREFERENCED(elementPtr)) {
    if (!elementPtr->isReferenced) {
        delete elementPtr;
    }
}

__attribute__((transaction_safe))
bool mesh_t::insertBoundary(edge_t* boundaryPtr)
{
    return custom_set_insertion(boundarySetPtr, boundaryPtr);
}

__attribute__((transaction_safe))
bool mesh_t::removeBoundary(edge_t* boundaryPtr)
{
    return boundarySetPtr->erase(boundaryPtr) == 1;
}

/* =============================================================================
 * createElement
 * =============================================================================
 */
static void
createElement (mesh_t* meshPtr,
               coordinate_t* coordinates,
               long numCoordinate,
               std::map<edge_t*, element_t*, element_mapCompareEdge_t>* edgeMapPtr)
{
    element_t* elementPtr = new element_t(coordinates, numCoordinate);
    assert(elementPtr);

    if (numCoordinate == 2) {
        edge_t* boundaryPtr = elementPtr->getEdge(0);
        bool status = custom_set_insertion(meshPtr->boundarySetPtr, boundaryPtr);
        assert(status);
    }

    meshPtr->insert(elementPtr, edgeMapPtr);

    if (elementPtr->isBad()) {
        meshPtr->initBadQueuePtr->push(elementPtr);
    }
 }


long mesh_t::read(const char* fileNamePrefix)
{
    FILE* inputFile;
    coordinate_t* coordinates;
    char fileName[256];
    long fileNameSize = sizeof(fileName) / sizeof(fileName[0]);
    char inputBuff[256];
    long inputBuffSize = sizeof(inputBuff) / sizeof(inputBuff[0]);
    long numEntry;
    long numDimension;
    long numCoordinate;
    long i;
    long numElement = 0;

    std::map<edge_t*, element_t*, element_mapCompareEdge_t>* edgeMapPtr =
        new std::map<edge_t*, element_t*, element_mapCompareEdge_t>();
    assert(edgeMapPtr);

    /*
     * Read .node file
     */
    snprintf(fileName, fileNameSize, "%s.node", fileNamePrefix);
    inputFile = fopen(fileName, "r");
    assert(inputFile);
    fgets(inputBuff, inputBuffSize, inputFile);
    sscanf(inputBuff, "%li %li", &numEntry, &numDimension);
    assert(numDimension == 2); /* must be 2-D */
    numCoordinate = numEntry + 1; /* numbering can start from 1 */
    coordinates = (coordinate_t*)malloc(numCoordinate * sizeof(coordinate_t));
    assert(coordinates);
    for (i = 0; i < numEntry; i++) {
        long id;
        double x;
        double y;
        if (!fgets(inputBuff, inputBuffSize, inputFile)) {
            break;
        }
        if (inputBuff[0] == '#') {
            continue; /* TODO: handle comments correctly */
        }
        sscanf(inputBuff, "%li %lf %lf", &id, &x, &y);
        coordinates[id].x = x;
        coordinates[id].y = y;
    }
    assert(i == numEntry);
    fclose(inputFile);

    /*
     * Read .poly file, which contains boundary segments
     */
    snprintf(fileName, fileNameSize, "%s.poly", fileNamePrefix);
    inputFile = fopen(fileName, "r");
    assert(inputFile);
    fgets(inputBuff, inputBuffSize, inputFile);
    sscanf(inputBuff, "%li %li", &numEntry, &numDimension);
    assert(numEntry == 0); /* .node file used for vertices */
    assert(numDimension == 2); /* must be edge */
    fgets(inputBuff, inputBuffSize, inputFile);
    sscanf(inputBuff, "%li", &numEntry);
    for (i = 0; i < numEntry; i++) {
        long id;
        long a;
        long b;
        coordinate_t insertCoordinates[2];
        if (!fgets(inputBuff, inputBuffSize, inputFile)) {
            break;
        }
        if (inputBuff[0] == '#') {
            continue; /* TODO: handle comments correctly */
        }
        sscanf(inputBuff, "%li %li %li", &id, &a, &b);
        assert(a >= 0 && a < numCoordinate);
        assert(b >= 0 && b < numCoordinate);
        insertCoordinates[0] = coordinates[a];
        insertCoordinates[1] = coordinates[b];
        createElement(this, insertCoordinates, 2, edgeMapPtr);
    }
    assert(i == numEntry);
    numElement += numEntry;
    fclose(inputFile);

    /*
     * Read .ele file, which contains triangles
     */
    snprintf(fileName, fileNameSize, "%s.ele", fileNamePrefix);
    inputFile = fopen(fileName, "r");
    assert(inputFile);
    fgets(inputBuff, inputBuffSize, inputFile);
    sscanf(inputBuff, "%li %li", &numEntry, &numDimension);
    assert(numDimension == 3); /* must be triangle */
    for (i = 0; i < numEntry; i++) {
        long id;
        long a;
        long b;
        long c;
        coordinate_t insertCoordinates[3];
        if (!fgets(inputBuff, inputBuffSize, inputFile)) {
            break;
        }
        if (inputBuff[0] == '#') {
            continue; /* TODO: handle comments correctly */
        }
        sscanf(inputBuff, "%li %li %li %li", &id, &a, &b, &c);
        assert(a >= 0 && a < numCoordinate);
        assert(b >= 0 && b < numCoordinate);
        assert(c >= 0 && c < numCoordinate);
        insertCoordinates[0] = coordinates[a];
        insertCoordinates[1] = coordinates[b];
        insertCoordinates[2] = coordinates[c];
        createElement(this, insertCoordinates, 3, edgeMapPtr);
    }
    assert(i == numEntry);
    numElement += numEntry;
    fclose(inputFile);

    free(coordinates);
    delete edgeMapPtr;

    return numElement;
}


/* =============================================================================
 * mesh_getBad
 * -- Returns NULL if none
 * =============================================================================
 */
element_t* mesh_t::getBad()
{
    if (initBadQueuePtr->empty())
        return NULL;
    auto res = initBadQueuePtr->front();
    initBadQueuePtr->pop();
    return res;
}

void mesh_t::shuffleBad(std::mt19937* randomPtr)
{
    // get the size of the queue
    long numElement = initBadQueuePtr->size();

    // move queue elements into a vector
    std::vector<element_t*> ev;
    while (!initBadQueuePtr->empty()) {
        ev.push_back(initBadQueuePtr->front());
        initBadQueuePtr->pop();
    }
    assert(initBadQueuePtr->empty());

    // shuffle the vector
    for (long i = 0; i < numElement; i++) {
        long r1 = randomPtr->operator()() % numElement;
        long r2 = randomPtr->operator()() % numElement;
        element_t* tmp = ev[r1];
        ev[r1] = ev[r2];
        ev[r2] = tmp;
    }

    // move data back into the queue
    for (auto i : ev) {
        initBadQueuePtr->push(i);
    }
}


/* =============================================================================
 * mesh_check
 * =============================================================================
 */
bool mesh_t::check(long expectedNumElement)
{
    std::queue<element_t*>* searchQueuePtr;
    std::map<element_t*, int, element_mapCompare_t>* visitedMapPtr;
    long numBadTriangle = 0;
    long numFalseNeighbor = 0;
    long numElement = 0;

    puts("Checking final mesh:");
    fflush(stdout);

    searchQueuePtr = new std::queue<element_t*>();
    assert(searchQueuePtr);
    visitedMapPtr = new std::map<element_t*, int, element_mapCompare_t>();
    assert(visitedMapPtr);

    /*
     * Do breadth-first search starting from rootElementPtr
     */
    assert(rootElementPtr);
    searchQueuePtr->push(rootElementPtr);
    while (!searchQueuePtr->empty()) {

        element_t* currentElementPtr;
        bool isSuccess;

        if (searchQueuePtr->empty()) {
            currentElementPtr = NULL;
        }
        else {
            currentElementPtr = searchQueuePtr->front();
            searchQueuePtr->pop();
        }
        if (visitedMapPtr->find(currentElementPtr) != visitedMapPtr->end()) {
            continue;
        }
        isSuccess = visitedMapPtr->insert(std::make_pair(currentElementPtr, NULL)).second;
        assert(isSuccess);
        if (!currentElementPtr->eltCheckAngles()) {
            numBadTriangle++;
        }
        auto neighborListPtr = currentElementPtr->getNeighborListPtr();

        for (auto it : *neighborListPtr) {
            element_t* neighborElementPtr = it;

            /*
             * Continue breadth-first search
             */
            if (visitedMapPtr->find(neighborElementPtr) == visitedMapPtr->end()) {
                searchQueuePtr->push(neighborElementPtr);
            }
        } /* for each neighbor */

        numElement++;

    } /* breadth-first search */

    printf("Number of elements      = %li\n", numElement);
    printf("Number of bad triangles = %li\n", numBadTriangle);

    delete searchQueuePtr;
    delete visitedMapPtr;

    return ((numBadTriangle > 0 ||
             numFalseNeighbor > 0 ||
             numElement != expectedNumElement) ? false : true);
}


#ifdef TEST_MESH
/* =============================================================================
 * TEST_MESH
 * =============================================================================
 */


#include <stdio.h>


int
main (int argc, char* argv[])
{
    mesh_t* meshPtr;

    assert(argc == 2);

    puts("Starting tests...");

    meshPtr = mesh_alloc();
    assert(meshPtr);

    mesh_read(meshPtr, argv[1]);

    mesh_free(meshPtr);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_MESH */
