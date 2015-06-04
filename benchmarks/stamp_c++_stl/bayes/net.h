/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <vector>
#include <set>
#include <queue>
#include <random>
#include "operation.h"

enum net_node_mark_t {
    NET_NODE_MARK_INIT = 0,
    NET_NODE_MARK_DONE = 1,
    NET_NODE_MARK_TEST = 2
};

struct net_node_t {
    long id;
    std::set<long>* parentIdListPtr;
    std::set<long>* childIdListPtr;
    net_node_mark_t mark;
};

struct net_t {
    std::vector<net_node_t*>* nodeVectorPtr;

    net_t(long numNode);
    ~net_t();
};

/* =============================================================================
 * TMnet_applyOperation
 * =============================================================================
 */
__attribute__((transaction_safe))
void
TMnet_applyOperation (net_t* netPtr, operation_t op, long fromId, long toId);


/* =============================================================================
 * TMnet_hasEdge
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
TMnet_hasEdge (  net_t* netPtr, long fromId, long toId);



/* =============================================================================
 * TMnet_isPath
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
TMnet_isPath (net_t* netPtr,
              long fromId,
              long toId,
              std::vector<bool>* visitedBitmapPtr,
              std::queue<long>* workQueuePtr);


/* =============================================================================
 * net_isCycle
 * =============================================================================
 */
bool
net_isCycle (net_t* netPtr);


/* =============================================================================
 * net_getParentIdListPtr
 * =============================================================================
 */
__attribute__((transaction_safe))
std::set<long>*
net_getParentIdListPtr (net_t* netPtr, long id);


/* =============================================================================
 * net_getChildIdListPtr
 * =============================================================================
 */
std::set<long>*
net_getChildIdListPtr (net_t* netPtr, long id);


/* =============================================================================
 * TMnet_findAncestors
 * -- Contents of bitmapPtr set to 1 if parent, else 0
 * -- Returns false if id is not root node (i.e., has cycle back id)
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
TMnet_findAncestors (net_t* netPtr,
                     long id,
                     std::vector<bool>* ancestorBitmapPtr,
                     std::queue<long>* workQueuePtr);


/* =============================================================================
 * TMnet_findDescendants
 * -- Contents of bitmapPtr set to 1 if descendants, else 0
 * -- Returns false if id is not root node (i.e., has cycle back id)
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
TMnet_findDescendants (net_t* netPtr,
                       long id,
                       std::vector<bool>* descendantBitmapPtr,
                       std::queue<long>* workQueuePtr);


/* =============================================================================
 * net_generateRandomEdges
 * =============================================================================
 */
void
net_generateRandomEdges (net_t* netPtr,
                         long maxNumParent,
                         long percentParent,
                         std::mt19937* randomPtr);
