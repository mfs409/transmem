/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <set>
#include "coordinate.h"
#include "grid.h"
#include "maze.h"

/* =============================================================================
 * maze_alloc
 * =============================================================================
 */
maze_t::maze_t()
{
    gridPtr = NULL;
    workQueuePtr = new std::queue<std::pair<coordinate_t*, coordinate_t*>*>();
    wallVectorPtr = new std::vector<coordinate_t*>();
    srcVectorPtr = new std::vector<coordinate_t*>();
    dstVectorPtr = new std::vector<coordinate_t*>();
    assert(workQueuePtr &&
           wallVectorPtr &&
           srcVectorPtr &&
           dstVectorPtr);
}

/* =============================================================================
 * maze_free
 * =============================================================================
 */
maze_t::~maze_t()
{
    if (gridPtr != NULL) {
        delete(gridPtr);
    }
    delete workQueuePtr;
    delete wallVectorPtr;
    while (!srcVectorPtr->empty()) {
        delete srcVectorPtr->back();
        srcVectorPtr->pop_back();
    }
    delete srcVectorPtr;
    while (!dstVectorPtr->empty()) {
        delete dstVectorPtr->back();
        dstVectorPtr->pop_back();
    }
    delete dstVectorPtr;
}


/* =============================================================================
 * addToGrid
 * =============================================================================
 */
static void
addToGrid (grid_t* gridPtr, std::vector<coordinate_t*>* vectorPtr, const char* type)
{
    long i;
    long n = vectorPtr->size();
    for (i = 0; i < n; i++) {
        coordinate_t* coordinatePtr = vectorPtr->at(i);
        if (!gridPtr->isPointValid(coordinatePtr->x,
                                   coordinatePtr->y,
                                   coordinatePtr->z))
        {
            fprintf(stderr, "Error: %s (%li, %li, %li) invalid\n",
                    type, coordinatePtr->x, coordinatePtr->y, coordinatePtr->z);
            exit(1);
        }
    }
    gridPtr->addPath(vectorPtr);
}

struct coord_compare
{
    bool operator()(const std::pair<coordinate_t*, coordinate_t*>* a, const std::pair<coordinate_t*, coordinate_t*>* b)
    {
        return coordinate_comparePair(a, b);
    }
};

/* =============================================================================
 * maze_read
 * -- Return number of path to route
 * =============================================================================
 */
long maze_t::read(const char* inputFileName)
{
    FILE* inputFile = fopen(inputFileName, "rt");
    if (!inputFile) {
        fprintf(stderr, "Error: Could not read %s\n", inputFileName);
        exit(1);
    }

    /*
     * Parse input file
     */
    long lineNumber = 0;
    long height = -1;
    long width  = -1;
    long depth  = -1;
    char line[256];
    std::multiset<std::pair<coordinate_t*, coordinate_t*>*, coord_compare>* workListPtr =
        new std::multiset<std::pair<coordinate_t*, coordinate_t*>*, coord_compare>();

    while (fgets(line, sizeof(line), inputFile)) {

        char code;
        long x1, y1, z1;
        long x2, y2, z2;
        long numToken = sscanf(line, " %c %li %li %li %li %li %li",
                              &code, &x1, &y1, &z1, &x2, &y2, &z2);

        lineNumber++;

        if (numToken < 1) {
            continue;
        }

        switch (code) {
            case '#': { /* comment */
                /* ignore line */
                break;
            }
            case 'd': { /* dimensions (format: d x y z) */
                if (numToken != 4) {
                    goto PARSE_ERROR;
                }
                width  = x1;
                height = y1;
                depth  = z1;
                if (width < 1 || height < 1 || depth < 1) {
                    goto PARSE_ERROR;
                }
                break;
            }
            case 'p': { /* paths (format: p x1 y1 z1 x2 y2 z2) */
                if (numToken != 7) {
                    goto PARSE_ERROR;
                }
                coordinate_t* srcPtr = new coordinate_t(x1, y1, z1);
                coordinate_t* dstPtr = new coordinate_t(x2, y2, z2);
                assert(srcPtr);
                assert(dstPtr);
                if (coordinate_isEqual(srcPtr, dstPtr)) {
                    goto PARSE_ERROR;
                }
                std::pair<coordinate_t*, coordinate_t*>* coordinatePairPtr =
                    new std::pair<coordinate_t*, coordinate_t*>(srcPtr, dstPtr);
                assert(coordinatePairPtr);
                unsigned int c = workListPtr->size();
                workListPtr->insert(coordinatePairPtr);
                assert((c + 1) == workListPtr->size());
                srcVectorPtr->push_back(srcPtr);
                dstVectorPtr->push_back(dstPtr);
                break;
            }
            case 'w': { /* walls (format: w x y z) */
                if (numToken != 4) {
                    goto PARSE_ERROR;
                }
                coordinate_t* wallPtr = new coordinate_t(x1, y1, z1);
                wallVectorPtr->push_back(wallPtr);
                break;
            }
            PARSE_ERROR:
            default: { /* error */
                fprintf(stderr, "Error: line %li of %s invalid\n",
                        lineNumber, inputFileName);
                exit(1);
            }
        }

    } /* iterate over lines in input file */

    fclose(inputFile);

    /*
     * Initialize grid contents
     */
    if (width < 1 || height < 1 || depth < 1) {
        fprintf(stderr, "Error: Invalid dimensions (%li, %li, %li)\n",
                width, height, depth);
        exit(1);
    }
    gridPtr = new grid_t(width, height, depth);
    assert(gridPtr);
    addToGrid(gridPtr, wallVectorPtr, "wall");
    addToGrid(gridPtr, srcVectorPtr,  "source");
    addToGrid(gridPtr, dstVectorPtr,  "destination");
    printf("Maze dimensions = %li x %li x %li\n", width, height, depth);
    printf("Paths to route  = %li\n", workListPtr->size());

    /*
     * Initialize work queue
     */
    for (auto i : *workListPtr) {
        workQueuePtr->push(i);
    }
    delete workListPtr;

    return srcVectorPtr->size();
}


/* =============================================================================
 * maze_checkPaths
 * =============================================================================
 */
bool maze_t::checkPaths(std::set<std::vector<std::vector<long*>*>*>* pathVectorListPtr,
                        bool doPrintPaths)
{
    long width  = gridPtr->width;
    long height = gridPtr->height;
    long depth  = gridPtr->depth;
    long i;

    /* Mark walls */
    grid_t* testGridPtr = new grid_t(width, height, depth);
    testGridPtr->addPath(wallVectorPtr);

    /* Mark sources */
    long numSrc = srcVectorPtr->size();
    for (i = 0; i < numSrc; i++) {
        coordinate_t* srcPtr = srcVectorPtr->at(i);
        testGridPtr->setPoint(srcPtr->x, srcPtr->y, srcPtr->z, 0);
    }

    /* Mark destinations */
    long numDst = dstVectorPtr->size();
    for (i = 0; i < numDst; i++) {
        coordinate_t* dstPtr = dstVectorPtr->at(i);
        testGridPtr->setPoint(dstPtr->x, dstPtr->y, dstPtr->z, 0);
    }

    /* Make sure path is contiguous and does not overlap */
    long id = 0;
    for (auto it : *pathVectorListPtr) {
        std::vector<std::vector<long*>*>* pathVectorPtr = it;
        long numPath = pathVectorPtr->size();
        long i;
        for (i = 0; i < numPath; i++) {
            id++;
            auto pointVectorPtr = pathVectorPtr->at(i);
            /* Check start */
            long* prevGridPointPtr = pointVectorPtr->at(0);
            long x;
            long y;
            long z;
            gridPtr->getPointIndices(prevGridPointPtr, &x, &y, &z);
            if (testGridPtr->getPoint(x, y, z) != 0) {
                delete testGridPtr;
                return false;
            }
            coordinate_t prevCoordinate(0,0,0);
            gridPtr->getPointIndices(prevGridPointPtr,
                                     &prevCoordinate.x,
                                     &prevCoordinate.y,
                                     &prevCoordinate.z);
            long numPoint = pointVectorPtr->size();
            long j;
            for (j = 1; j < (numPoint-1); j++) { /* no need to check endpoints */
                long* currGridPointPtr = pointVectorPtr->at(j);
                coordinate_t currCoordinate(0,0,0);
                gridPtr->getPointIndices(currGridPointPtr,
                                         &currCoordinate.x,
                                         &currCoordinate.y,
                                         &currCoordinate.z);
                if (!coordinate_areAdjacent(&currCoordinate, &prevCoordinate)) {
                    delete testGridPtr;
                    return false;
                }
                prevCoordinate = currCoordinate;
                long x = currCoordinate.x;
                long y = currCoordinate.y;
                long z = currCoordinate.z;
                if (testGridPtr->getPoint(x, y, z) != GRID_POINT_EMPTY) {
                    delete testGridPtr;
                    return false;
                } else {
                    testGridPtr->setPoint(x, y, z, id);
                }
            }
            /* Check end */
            long* lastGridPointPtr = pointVectorPtr->at(j);
            gridPtr->getPointIndices(lastGridPointPtr, &x, &y, &z);
            if (testGridPtr->getPoint(x, y, z) != 0) {
                delete testGridPtr;
                return false;
            }
        } /* iteratate over pathVector */
    } /* iterate over pathVectorList */

    if (doPrintPaths) {
        puts("\nRouted Maze:");
        testGridPtr->print();
    }

    delete testGridPtr;

    return true;
}
