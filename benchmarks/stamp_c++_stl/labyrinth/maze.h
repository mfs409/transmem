/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <vector>
#include <set>
#include <queue>
#include "coordinate.h"
#include "grid.h"

struct maze_t {
    grid_t* gridPtr;
    std::queue<std::pair<coordinate_t*, coordinate_t*>*>* workQueuePtr;   /* contains source/destination pairs to route */
    std::vector<coordinate_t*>* wallVectorPtr; /* obstacles */
    std::vector<coordinate_t*>* srcVectorPtr;  /* sources */
    std::vector<coordinate_t*>* dstVectorPtr;  /* destinations */

    maze_t();
    ~maze_t();

    /*
     * maze_read: Return number of path to route
     */
    long read(const char* inputFileName);

    bool checkPaths(std::set<std::vector<std::vector<long*>*>*>* pathListPtr, bool doPrintPaths);
};
