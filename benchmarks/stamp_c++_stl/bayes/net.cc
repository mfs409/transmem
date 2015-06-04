/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <stdlib.h>
#include "learner.h"
#include "net.h"
#include "operation.h"
#include "tm_transition.h"

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */

__attribute__((transaction_safe))
void
TMinsertEdge (  net_t* netPtr, long fromId, long toId);

__attribute__((transaction_safe))
void
TMremoveEdge (  net_t* netPtr, long fromId, long toId);

__attribute__((transaction_safe))
void
TMreverseEdge (  net_t* netPtr, long fromId, long toId);

/* =============================================================================
 * allocNode
 * =============================================================================
 */
static net_node_t*
allocNode (long id)
{
    net_node_t* nodePtr;

    nodePtr = (net_node_t*)malloc(sizeof(net_node_t));
    if (nodePtr) {
        nodePtr->parentIdListPtr = new std::set<long>();
        if (nodePtr->parentIdListPtr == NULL) {
            free(nodePtr);
            return NULL;
        }
        nodePtr->childIdListPtr = new std::set<long>();
        if (nodePtr->childIdListPtr == NULL) {
            delete nodePtr->parentIdListPtr;
            free(nodePtr);
            return NULL;
        }
        nodePtr->id = id;
    }

    return nodePtr;
}


/* =============================================================================
 * freeNode
 * =============================================================================
 */
static void
freeNode (net_node_t* nodePtr)
{
    delete nodePtr->childIdListPtr;
    delete nodePtr->parentIdListPtr;
    free(nodePtr);
}


/* =============================================================================
 * net_alloc
 * =============================================================================
 */
net_t::net_t(long numNode)
{
    nodeVectorPtr = new std::vector<net_node_t*>();
    if (nodeVectorPtr == NULL) {
        assert(false);
    }
    nodeVectorPtr->reserve(numNode);
    for (long i = 0; i < numNode; i++) {
        net_node_t* nodePtr = allocNode(i);
        if (nodePtr == NULL) {
            for (long j = 0; j < i; j++) {
                nodePtr = nodeVectorPtr->at(j);
                freeNode(nodePtr);
            }
            delete nodeVectorPtr;
            assert(false);
        }
        nodeVectorPtr->push_back(nodePtr);
    }
}


/* =============================================================================
 * net_free
 * =============================================================================
 */
net_t::~net_t()
{
    long numNode = nodeVectorPtr->size();
    for (long i = 0; i < numNode; i++) {
        net_node_t* nodePtr = nodeVectorPtr->at(i);
        freeNode(nodePtr);
    }
    delete nodeVectorPtr;
}

/* =============================================================================
 * TMinsertEdge
 * =============================================================================
 */
__attribute__((transaction_safe))
void
TMinsertEdge (  net_t* netPtr, long fromId, long toId)
{
    std::vector<net_node_t*>* nodeVectorPtr = netPtr->nodeVectorPtr;
    bool status;

    net_node_t* childNodePtr = nodeVectorPtr->at(toId);
    std::set<long>* parentIdListPtr = childNodePtr->parentIdListPtr;
    status = parentIdListPtr->insert(fromId).second;

    net_node_t* parentNodePtr = nodeVectorPtr->at(fromId);
    std::set<long>* childIdListPtr = parentNodePtr->childIdListPtr;
    status = childIdListPtr->insert(toId).second;
    assert(status);
}


/* =============================================================================
 * TMremoveEdge
 * =============================================================================
 */
__attribute__((transaction_safe))
void
TMremoveEdge (  net_t* netPtr, long fromId, long toId)
{
    std::vector<net_node_t*>* nodeVectorPtr = netPtr->nodeVectorPtr;
    bool status;

    net_node_t* childNodePtr = nodeVectorPtr->at(toId);
    std::set<long>* parentIdListPtr = childNodePtr->parentIdListPtr;
    status = parentIdListPtr->erase(fromId) == 1;
    assert(status);

    net_node_t* parentNodePtr = nodeVectorPtr->at(fromId);
    std::set<long>* childIdListPtr = parentNodePtr->childIdListPtr;
    status = 1 == childIdListPtr->erase(toId);
    assert(status);
}

/* =============================================================================
 * TMreverseEdge
 * =============================================================================
 */
__attribute__((transaction_safe))
void
TMreverseEdge (  net_t* netPtr, long fromId, long toId)
{
    TMremoveEdge(  netPtr, fromId, toId);
    TMinsertEdge(  netPtr, toId, fromId);
}


/* =============================================================================
 * TMnet_applyOperation
 * =============================================================================
 */
__attribute__((transaction_safe))
void
TMnet_applyOperation (net_t* netPtr, operation_t op, long fromId, long toId)
{
  switch (op) {
   case OPERATION_INSERT:  TMinsertEdge(netPtr, fromId, toId); break;
   case OPERATION_REMOVE:  TMremoveEdge(netPtr, fromId, toId); break;
   case OPERATION_REVERSE: TMreverseEdge(netPtr, fromId, toId); break;
   default:
    assert(0);
  }
}

/* =============================================================================
 * TMnet_hasEdge
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
TMnet_hasEdge (  net_t* netPtr, long fromId, long toId)
{
    std::vector<net_node_t*>* nodeVectorPtr = netPtr->nodeVectorPtr;
    net_node_t* childNodePtr = nodeVectorPtr->at(toId);
    std::set<long>* parentIdListPtr = childNodePtr->parentIdListPtr;

    for (auto it : *parentIdListPtr) {
        long parentId = it;
        if (parentId == fromId)
            return true;
    }
    return false;
}


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
              std::queue<long>* workQueuePtr)
{
    std::vector<net_node_t*>* nodeVectorPtr = netPtr->nodeVectorPtr;
    assert(visitedBitmapPtr->size() == nodeVectorPtr->size());
    //TM_SAFE
    for (auto i : *visitedBitmapPtr)
        i = false;
    while (!workQueuePtr->empty())
        workQueuePtr->pop();

    //[wer] below QUEUE_xxx were all TM-pure version previously
    workQueuePtr->push(fromId);

    while (!workQueuePtr->empty()) {
        long id = workQueuePtr->front();
        workQueuePtr->pop();
        if (id == toId) {
            while (!workQueuePtr->empty())
                workQueuePtr->pop();
            return true;
        }

        visitedBitmapPtr->at(id) = true;

        net_node_t* nodePtr = nodeVectorPtr->at(id);
        std::set<long>* childIdListPtr = nodePtr->childIdListPtr;
        for (auto it : *childIdListPtr) {
            long childId = it;
            if (!visitedBitmapPtr->at(childId)) {
                workQueuePtr->push(childId);
            }
        }
    }
    return false;
}


/* =============================================================================
 * isCycle
 * =============================================================================
 */
static bool
isCycle (std::vector<net_node_t*>* nodeVectorPtr, net_node_t* nodePtr)
{
    switch (nodePtr->mark) {
        case NET_NODE_MARK_INIT: {
            nodePtr->mark = NET_NODE_MARK_TEST;
            std::set<long>* childIdListPtr = nodePtr->childIdListPtr;
            for (auto it : *childIdListPtr) {
                long childId = it;
                net_node_t* childNodePtr = nodeVectorPtr->at(childId);
                if (isCycle(nodeVectorPtr, childNodePtr)) {
                    return true;
                }
            }
            break;
        }
        case NET_NODE_MARK_TEST:
            return true;
        case NET_NODE_MARK_DONE:
            return false;
            break;
        default:
            assert(0);
    }

    nodePtr->mark = NET_NODE_MARK_DONE;
    return false;
}


/* =============================================================================
 * net_isCycle
 * =============================================================================
 */
bool
net_isCycle (net_t* netPtr)
{
    std::vector<net_node_t*>* nodeVectorPtr = netPtr->nodeVectorPtr;
    long numNode = nodeVectorPtr->size();
    long n;
    for (n = 0; n < numNode; n++) {
        net_node_t* nodePtr = nodeVectorPtr->at(n);
        nodePtr->mark = NET_NODE_MARK_INIT;
    }

    for (n = 0; n < numNode; n++) {
        net_node_t* nodePtr = nodeVectorPtr->at(n);
        switch (nodePtr->mark) {
            case NET_NODE_MARK_INIT:
                if (isCycle(nodeVectorPtr, nodePtr)) {
                    return true;
                }
                break;
            case NET_NODE_MARK_DONE:
                /* do nothing */
                break;
            case NET_NODE_MARK_TEST:
                assert(0);
                break;
            default:
                assert(0);
                break;
        }
    }

    return false;
}


/* =============================================================================
 * net_getParentIdListPtr
 * =============================================================================
 */
//TM_PURE
__attribute__((transaction_safe))
std::set<long>*
net_getParentIdListPtr (net_t* netPtr, long id)
{
    net_node_t* nodePtr = netPtr->nodeVectorPtr->at(id);
    assert(nodePtr);

    return nodePtr->parentIdListPtr;
}


/* =============================================================================
 * net_getChildIdListPtr
 * =============================================================================
 */
std::set<long>*
net_getChildIdListPtr (net_t* netPtr, long id)
{
    net_node_t* nodePtr = netPtr->nodeVectorPtr->at(id);
    assert(nodePtr);

    return nodePtr->childIdListPtr;
}



/* =============================================================================
 * TMnet_findAncestors
 * -- Contents of bitmapPtr set to 1 if ancestor, else 0
 * -- Returns false if id is not root node (i.e., has cycle back id)
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
TMnet_findAncestors (net_t* netPtr,
                     long id,
                     std::vector<bool>* ancestorBitmapPtr,
                     std::queue<long>* workQueuePtr)
{
    std::vector<net_node_t*>* nodeVectorPtr = netPtr->nodeVectorPtr;
    assert(ancestorBitmapPtr->size() == nodeVectorPtr->size());

    for (auto i : *ancestorBitmapPtr)
        i = false;

    while (!workQueuePtr->empty())
        workQueuePtr->pop();

    {
        net_node_t* nodePtr = nodeVectorPtr->at(id);
        std::set<long>* parentIdListPtr = nodePtr->parentIdListPtr;
        for (auto it : *parentIdListPtr) {
            long parentId = it;

            ancestorBitmapPtr->at(parentId) = true;
            workQueuePtr->push(parentId);
        }
    }

    while (!workQueuePtr->empty()) {
        long parentId = workQueuePtr->front();
        workQueuePtr->pop();
        if (parentId == id) {
            while (!workQueuePtr->empty())
                workQueuePtr->pop();
            return false;
        }
        net_node_t* nodePtr = nodeVectorPtr->at(parentId);
        std::set<long>* grandParentIdListPtr = nodePtr->parentIdListPtr;
        for (auto it : *grandParentIdListPtr) {
            long grandParentId = it;
            if (!ancestorBitmapPtr->at(grandParentId)) {
                ancestorBitmapPtr->at(grandParentId) = true;
                workQueuePtr->push(grandParentId);
            }
        }
    }

    return true;
}


/* =============================================================================
 * TMnet_findDescendants
 * -- Contents of bitmapPtr set to 1 if descendants, else 0
 * -- Returns false if id is not root node (i.e., has cycle back id)
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
TMnet_findDescendants(net_t* netPtr,
                      long id,
                      std::vector<bool>* descendantBitmapPtr,
                      std::queue<long>* workQueuePtr)
{
    std::vector<net_node_t*>* nodeVectorPtr = netPtr->nodeVectorPtr;
    assert(descendantBitmapPtr->size() == nodeVectorPtr->size());

    for (auto i : *descendantBitmapPtr)
        i = false;

    while (!workQueuePtr->empty())
        workQueuePtr->pop();

    net_node_t* nodePtr = nodeVectorPtr->at(id);
    std::set<long>* childIdListPtr = nodePtr->childIdListPtr;
    for (auto it : *childIdListPtr) {
        long childId = it;

        descendantBitmapPtr->at(childId) = true;
        //[wer] all QUEUE_XXs were PQUEUE before
        workQueuePtr->push(childId);
    }

    while (!workQueuePtr->empty()) {
        long childId = workQueuePtr->front();
        workQueuePtr->pop();
        if (childId == id) {
            while (!workQueuePtr->empty())
                workQueuePtr->pop();
            return false;
        }
        net_node_t* nodePtr = nodeVectorPtr->at(childId);
        std::set<long>* grandChildIdListPtr = nodePtr->childIdListPtr;
        for (auto it : *grandChildIdListPtr) {
            long grandChildId = it;

            if (!descendantBitmapPtr->at(grandChildId)) {
                descendantBitmapPtr->at(grandChildId) = true;
                workQueuePtr->push(grandChildId);
            }
        }
    }

    return true;
}


/* =============================================================================
 * net_generateRandomEdges
 * =============================================================================
 */
void
net_generateRandomEdges (net_t* netPtr,
                         long maxNumParent,
                         long percentParent,
                         std::mt19937* randomPtr)
{
    std::vector<net_node_t*>* nodeVectorPtr = netPtr->nodeVectorPtr;

    long numNode = nodeVectorPtr->size();
    std::vector<bool>* visitedBitmapPtr = new std::vector<bool>(numNode);
    assert(visitedBitmapPtr);
    std::queue<long>* workQueuePtr = new std::queue<long>();

    long n;

    for (n = 0; n < numNode; n++) {
        long p;
        for (p = 0; p < maxNumParent; p++) {
            long value = randomPtr->operator()() % 100;
            if (value < percentParent) {
                long parent = randomPtr->operator()() % numNode;
                if ((parent != n) &&
                    !TMnet_hasEdge(netPtr, parent, n) &&
                    !TMnet_isPath(netPtr, n, parent, visitedBitmapPtr, workQueuePtr))
                {
#ifdef TEST_NET
                    printf("node=%li parent=%li\n", n, parent);
#endif
                    TMinsertEdge(netPtr, parent, n);
                }
            }
        }
    }

    assert(!net_isCycle(netPtr));

    delete visitedBitmapPtr;
    delete workQueuePtr;
}


/* #############################################################################
 * TEST_NET
 * #############################################################################
 */
#ifdef TEST_NET

#include <stdio.h>


int
main ()
{
    long numNode = 100;

    puts("Starting tests...");

    bool status;

    net_t* netPtr = net_alloc(numNode);
    assert(netPtr);
    bitmap_t* visitedBitmapPtr = (bitmap_t*)bitmap_alloc(numNode);
    assert(visitedBitmapPtr);
    std::queue<long>* workQueuePtr = queue_alloc(-1);
    assert(workQueuePtr);

    assert(!net_isCycle(netPtr));

    long aId = 31;
    long bId = 14;
    long cId = 5;
    long dId = 92;

    TMnet_applyOperation(netPtr, OPERATION_INSERT, aId, bId);
    assert(TMnet_isPath(netPtr, aId, bId, visitedBitmapPtr, workQueuePtr));
    assert(!TMnet_isPath(netPtr, bId, aId, visitedBitmapPtr, workQueuePtr));
    assert(!TMnet_isPath(netPtr, aId, cId, visitedBitmapPtr, workQueuePtr));
    assert(!TMnet_isPath(netPtr, aId, dId, visitedBitmapPtr, workQueuePtr));
    assert(!net_isCycle(netPtr));

    TMnet_applyOperation(netPtr, OPERATION_INSERT, bId, cId);
    TMnet_applyOperation(netPtr, OPERATION_INSERT, aId, cId);
    TMnet_applyOperation(netPtr, OPERATION_INSERT, dId, aId);
    assert(!net_isCycle(netPtr));
    TMnet_applyOperation(netPtr, OPERATION_INSERT, cId, dId);
    assert(net_isCycle(netPtr));
    TMnet_applyOperation(netPtr, OPERATION_REVERSE, cId, dId);
    assert(!net_isCycle(netPtr));
    TMnet_applyOperation(netPtr, OPERATION_REVERSE, dId, cId);
    assert(net_isCycle(netPtr));
    assert(TMnet_isPath(netPtr, aId, dId, visitedBitmapPtr, workQueuePtr));
    TMnet_applyOperation(netPtr, OPERATION_REMOVE, cId, dId);
    assert(!TMnet_isPath(netPtr, aId, dId, visitedBitmapPtr, workQueuePtr));

    bitmap_t* ancestorBitmapPtr = (bitmap_t*)bitmap_alloc(numNode);
    assert(ancestorBitmapPtr);
    status = TMnet_findAncestors(netPtr, cId, ancestorBitmapPtr, workQueuePtr);
    assert(status);
    assert(bitmap_isSet(ancestorBitmapPtr, aId));
    assert(bitmap_isSet(ancestorBitmapPtr, bId));
    assert(bitmap_isSet(ancestorBitmapPtr, dId));
    assert(bitmap_getNumSet(ancestorBitmapPtr) == 3);

    bitmap_t* descendantBitmapPtr = (bitmap_t*)bitmap_alloc(numNode);
    assert(descendantBitmapPtr);
    status = TMnet_findDescendants(netPtr, aId, descendantBitmapPtr, workQueuePtr);
    assert(status);
    assert(bitmap_isSet(descendantBitmapPtr, bId));
    assert(bitmap_isSet(descendantBitmapPtr, cId));
    assert(bitmap_getNumSet(descendantBitmapPtr) == 2);

    bitmap_free(visitedBitmapPtr);
    queue_free(workQueuePtr);
    bitmap_free(ancestorBitmapPtr);
    bitmap_free(descendantBitmapPtr);
    net_free(netPtr);

    random_t* randomPtr = random_alloc();
    assert(randomPtr);
    netPtr = net_alloc(numNode);
    assert(netPtr);
    net_generateRandomEdges(netPtr, 10, 10, randomPtr);
    net_free(netPtr);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_NET */
