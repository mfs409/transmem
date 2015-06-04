/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/* =============================================================================
 *
 * learn.c
 * -- Learns structure of Bayesian net from data
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * The penalized log-likelihood score (Friedman & Yahkani, 1996) is used to
 * evaluated the "goodness" of a Bayesian net:
 *
 *                             M      n_j
 *                            --- --- ---
 *  -N_params * ln(R) / 2 + R >   >   >   P((a_j = v), X_j) ln P(a_j = v | X_j)
 *                            --- --- ---
 *                            j=1 X_j v=1
 *
 * Where:
 *
 *     N_params     total number of parents across all variables
 *     R            number of records
 *     M            number of variables
 *     X_j          parents of the jth variable
 *     n_j          number of attributes of the jth variable
 *     a_j          attribute
 *
 * The second summation of X_j varies across all possible assignments to the
 * values of the parents X_j.
 *
 * In the code:
 *
 *    "local log likelihood" is  P((a_j = v), X_j) ln P(a_j = v | X_j)
 *    "log likelihood" is everything to the right of the '+', i.e., "R ... X_j)"
 *    "base penalty" is -ln(R) / 2
 *    "penalty" is N_params * -ln(R) / 2
 *    "score" is the entire expression
 *
 * For more notes, refer to:
 *
 * A. Moore and M.-S. Lee. Cached sufficient statistics for efficient machine
 * learning with large datasets. Journal of Artificial Intelligence Research 8
 * (1998), pp 67-91.
 *
 * =============================================================================
 *
 * The search strategy uses a combination of local and global structure search.
 * Similar to the technique described in:
 *
 * D. M. Chickering, D. Heckerman, and C. Meek.  A Bayesian approach to learning
 * Bayesian networks with local structure. In Proceedings of Thirteenth
 * Conference on Uncertainty in Artificial Intelligence (1997), pp. 80-89.
 *
 * =============================================================================
 */


#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <random>
#include "adtree.h"
#include "data.h"
#include "learner.h"
#include "net.h"
#include "operation.h"
#include "query.h"
#include "thread.h"
#include "timer.h"
#include "utility.h"
#include "tm_transition.h"
#include <algorithm>
#include "tm_hacks.h"

struct learner_task_t {
    operation_t op;
    long fromId;
    long toId;
    float score;
};

struct findBestTaskArg_t {
    long toId;
    learner_t* learnerPtr;
    query_t* queries;
    std::vector<query_t*>* queryVectorPtr;
    std::vector<query_t*>* parentQueryVectorPtr;
    long numTotalParent;
    float basePenalty;
    float baseLogLikelihood;
    std::vector<bool>* bitmapPtr;
    std::queue<long>* workQueuePtr;
    std::vector<query_t*>* aQueryVectorPtr;
    std::vector<query_t*>* bQueryVectorPtr;
};

#ifdef TEST_LEARNER
long global_maxNumEdgeLearned = -1L;
long global_insertPenalty = 1;
float global_operationQualityFactor = 1.0F;
#else
extern long global_insertPenalty;
extern long global_maxNumEdgeLearned;
extern float global_operationQualityFactor;
#endif


/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */

__attribute__((transaction_safe))
void
TMfindBestReverseTask (learner_task_t *dest,  findBestTaskArg_t* argPtr);

__attribute__((transaction_safe))
void
TMfindBestInsertTask (learner_task_t *dest,  findBestTaskArg_t* argPtr);

__attribute__((transaction_safe))
void
TMfindBestRemoveTask (learner_task_t *dest,  findBestTaskArg_t* argPtr);

__attribute__((transaction_safe))
void
TMpopulateParentQueryVector (net_t* netPtr,
                             long id,
                             query_t* queries,
                             std::vector<query_t*>* parentQueryVectorPtr);

__attribute__((transaction_safe))
void
TMpopulateQueryVectors (net_t* netPtr,
                        long id,
                        query_t* queries,
                        std::vector<query_t*>* queryVectorPtr,
                        std::vector<query_t*>* parentQueryVectorPtr);

__attribute__((transaction_safe))
learner_task_t*
TMpopTask (std::set<learner_task_t*, compareTask_t>* taskListPtr);

/* =============================================================================
 * compareTask
 * -- Want greatest score first
 * -- For list
 * =============================================================================
 */
//[wer] comparator
__attribute__((transaction_safe)) long
compareTask (const void* aPtr, const void* bPtr)
{
    learner_task_t* aTaskPtr = (learner_task_t*)aPtr;
    learner_task_t* bTaskPtr = (learner_task_t*)bPtr;
    float aScore = aTaskPtr->score;
    float bScore = bTaskPtr->score;

    if (aScore < bScore) {
        return 1;
    } else if (aScore > bScore) {
        return -1;
    } else {
        return (aTaskPtr->toId - bTaskPtr->toId);
    }
}

bool compareTask_t::operator()(learner_task_t* l, learner_task_t* r)
{
    return -1 == compareTask(l, r);
}

/* =============================================================================
 * compareQuery
 * -- Want smallest ID first
 * -- For vector_sort
 * =============================================================================
 */
__attribute__((transaction_safe))
int compareQuery (const void* aPtr, const void* bPtr)
{
    query_t* aQueryPtr = (query_t*)(*(void**)aPtr);
    query_t* bQueryPtr = (query_t*)(*(void**)bPtr);

    return (aQueryPtr->index - bQueryPtr->index);
}

struct compareQuery_t
{
    bool operator()(query_t* l, query_t* r)
    {
        return l->index < r->index;
    }
};

/* =============================================================================
 * learner_alloc
 * =============================================================================
 */
learner_t*
learner_alloc (data_t* dataPtr, adtree_t* adtreePtr)
{
    learner_t* learnerPtr;

    learnerPtr = (learner_t*)malloc(sizeof(learner_t));
    if (learnerPtr) {
        learnerPtr->adtreePtr = adtreePtr;
        learnerPtr->netPtr = new net_t(dataPtr->numVar);
        assert(learnerPtr->netPtr);
        learnerPtr->localBaseLogLikelihoods =
            (float*)malloc(dataPtr->numVar * sizeof(float));
        assert(learnerPtr->localBaseLogLikelihoods);
        learnerPtr->baseLogLikelihood = 0.0F;
        learnerPtr->tasks =
            (learner_task_t*)malloc(dataPtr->numVar * sizeof(learner_task_t));
        assert(learnerPtr->tasks);
        learnerPtr->taskListPtr = new std::set<learner_task_t*, compareTask_t>();
        assert(learnerPtr->taskListPtr);
        learnerPtr->numTotalParent = 0;
    }

    return learnerPtr;
}


/* =============================================================================
 * learner_free
 * =============================================================================
 */
void
learner_free (learner_t* learnerPtr)
{
    delete learnerPtr->taskListPtr;
    free(learnerPtr->tasks);
    free(learnerPtr->localBaseLogLikelihoods);
    delete learnerPtr->netPtr;
    free(learnerPtr);
}

/* =============================================================================
 * Logarithm(s)
 * =============================================================================
 */
__attribute__((transaction_safe))
double my_exp(double x, int y)
{
    double ret = 1;
    int i;
    for (i = 0; i < y; i++)
        ret *= x;
    return ret;
}

// Taylor series to calculate __attribute__((transaction_safe)) logarithm
__attribute__((transaction_safe))
double log_tm (double x) {
    assert (x > 0);
    double ret = 0.0;

    if (ABS(x - 1) <= 0.000001)
        return ret;
    else if (ABS(x) > 1) {
        double y = x / (x - 1);
        // more iteration, more precise
        for (int i = 1; i < 20; i++) {
            ret += 1 / (i * my_exp (y, i));
        }
        return ret;
    }
    else{
        double y = x - 1;
        // more iteration, more precise
        for (int i = 1; i < 20; i++) {
            if (i % 2 == 1)
                ret += my_exp (y, i) / i;
            else
                ret -= my_exp (y, i) / i;
        }
        return ret;
    }
}

/* =============================================================================
 * computeSpecificLocalLogLikelihood
 * -- Query vectors should not contain wildcards
 * =============================================================================
 */
__attribute__((transaction_safe))
float
computeSpecificLocalLogLikelihood (adtree_t* adtreePtr,
                                   std::vector<query_t*>* queryVectorPtr,
                                   std::vector<query_t*>* parentQueryVectorPtr)
{
  //[wer] __attribute__((transaction_safe)) call
  long count = adtree_getCount(adtreePtr, queryVectorPtr);
  if (count == 0) {
    return 0.0;
  }

  double probability = (double)count / (double)adtreePtr->numRecord;
  long parentCount = adtree_getCount(adtreePtr, parentQueryVectorPtr);

  assert(parentCount >= count);
  assert(parentCount > 0);

  double temp = (double)count/ (double)parentCount;
  double t = (double)log_tm(temp);
  return (float)(probability * t);
}


/* =============================================================================
 * createPartition
 * =============================================================================
 */
void
createPartition (long min, long max, long id, long n,
                 long* startPtr, long* stopPtr)
{
    long range = max - min;
    long chunk = MAX(1, ((range + n/2) / n)); /* rounded */
    long start = min + chunk * id;
    long stop;
    if (id == (n-1)) {
        stop = max;
    } else {
        stop = MIN(max, (start + chunk));
    }

    *startPtr = start;
    *stopPtr = stop;
}


/* =============================================================================
 * createTaskList
 * -- baseLogLikelihoods and taskListPtr are updated
 * =============================================================================
 */
void
createTaskList (void* argPtr)
{
    long myId = thread_getId();
    long numThread = thread_getNumThread();

    learner_t* learnerPtr = (learner_t*)argPtr;
    auto taskListPtr = learnerPtr->taskListPtr;

    bool status;

    adtree_t* adtreePtr = learnerPtr->adtreePtr;
    float* localBaseLogLikelihoods = learnerPtr->localBaseLogLikelihoods;
    learner_task_t* tasks = learnerPtr->tasks;

    query_t queries[2];
    std::vector<query_t*>* queryVectorPtr = make_vector_query();
    assert(queryVectorPtr);
    vector_query_push(queryVectorPtr, &queries[0]);

    query_t parentQuery;
    std::vector<query_t*>* parentQueryVectorPtr = make_vector_query();
    assert(parentQueryVectorPtr);

    long numVar = adtreePtr->numVar;
    long numRecord = adtreePtr->numRecord;
    float baseLogLikelihood = 0.0;
    float penalty = (float)(-0.5 * log((double)numRecord)); /* only add 1 edge */

    long v;

    long v_start;
    long v_stop;
    createPartition(0, numVar, myId, numThread, &v_start, &v_stop);

    /*
     * Compute base log likelihood for each variable and total base loglikelihood
     */

    for (v = v_start; v < v_stop; v++) {

        float localBaseLogLikelihood = 0.0;
        queries[0].index = v;

        queries[0].value = 0;
        localBaseLogLikelihood +=
            computeSpecificLocalLogLikelihood(adtreePtr,
                                              queryVectorPtr,
                                              parentQueryVectorPtr);

        queries[0].value = 1;
        localBaseLogLikelihood +=
            computeSpecificLocalLogLikelihood(adtreePtr,
                                              queryVectorPtr,
                                              parentQueryVectorPtr);

        localBaseLogLikelihoods[v] = localBaseLogLikelihood;
        baseLogLikelihood += localBaseLogLikelihood;

    } /* foreach variable */

    __transaction_atomic {
      float globalBaseLogLikelihood = learnerPtr->baseLogLikelihood;
      learnerPtr->baseLogLikelihood =
                        baseLogLikelihood + globalBaseLogLikelihood;
   }

    /*
     * For each variable, find if the addition of any edge _to_ it is better
     */

    vector_query_push(parentQueryVectorPtr, &parentQuery);

    for (v = v_start; v < v_stop; v++) {

        /*
         * Compute base log likelihood for this variable
         */

        queries[0].index = v;
        long bestLocalIndex = v;
        float bestLocalLogLikelihood = localBaseLogLikelihoods[v];

        vector_query_push(queryVectorPtr, &queries[1]);

        long vv;
        for (vv = 0; vv < numVar; vv++) {

            if (vv == v) {
                continue;
            }
            parentQuery.index = vv;
            if (v < vv) {
                queries[0].index = v;
                queries[1].index = vv;
            } else {
                queries[0].index = vv;
                queries[1].index = v;
            }

            float newLocalLogLikelihood = 0.0;

            queries[0].value = 0;
            queries[1].value = 0;
            parentQuery.value = 0;
            newLocalLogLikelihood +=
                computeSpecificLocalLogLikelihood(adtreePtr,
                                                  queryVectorPtr,
                                                  parentQueryVectorPtr);

            queries[0].value = 0;
            queries[1].value = 1;
            parentQuery.value = ((vv < v) ? 0 : 1);
            newLocalLogLikelihood +=
                computeSpecificLocalLogLikelihood(adtreePtr,
                                                  queryVectorPtr,
                                                  parentQueryVectorPtr);

            queries[0].value = 1;
            queries[1].value = 0;
            parentQuery.value = ((vv < v) ? 1 : 0);
            newLocalLogLikelihood +=
                computeSpecificLocalLogLikelihood(adtreePtr,
                                                  queryVectorPtr,
                                                  parentQueryVectorPtr);

            queries[0].value = 1;
            queries[1].value = 1;
            parentQuery.value = 1;
            newLocalLogLikelihood +=
                computeSpecificLocalLogLikelihood(adtreePtr,
                                                  queryVectorPtr,
                                                  parentQueryVectorPtr);

            if (newLocalLogLikelihood > bestLocalLogLikelihood) {
                bestLocalIndex = vv;
                bestLocalLogLikelihood = newLocalLogLikelihood;
            }

        } /* foreach other variable */

        queryVectorPtr->pop_back();

        if (bestLocalIndex != v) {
            float logLikelihood = numRecord * (baseLogLikelihood +
                                                + bestLocalLogLikelihood
                                                - localBaseLogLikelihoods[v]);
            float score = penalty + logLikelihood;
            learner_task_t* taskPtr = &tasks[v];
            taskPtr->op = OPERATION_INSERT;
            taskPtr->fromId = bestLocalIndex;
            taskPtr->toId = v;
            taskPtr->score = score;
            __transaction_atomic {
              status = taskListPtr->insert(taskPtr).second;
            }
            assert(status);
        }

    } /* for each variable */

    delete queryVectorPtr;
    delete parentQueryVectorPtr;

#ifdef TEST_LEARNER
    list_iter_t it;
    list_iter_reset(&it, taskListPtr);
    while (list_iter_hasNext(&it)) {
        learner_task_t* taskPtr = (learner_task_t*)list_iter_next(&it, taskListPtr);
        printf("[task] op=%i from=%li to=%li score=%lf\n",
               taskPtr->op, taskPtr->fromId, taskPtr->toId, taskPtr->score);
    }
#endif /* TEST_LEARNER */

}


/* =============================================================================
 * TMpopTask
 * -- Returns NULL is list is empty
 * =============================================================================
 */
__attribute__((transaction_safe))
learner_task_t*
TMpopTask (std::set<learner_task_t*, compareTask_t>* taskListPtr)
{
    learner_task_t* taskPtr = NULL;
    if (taskListPtr->size() > 0) {
        auto i = taskListPtr->begin();
        taskPtr = *i;
        taskListPtr->erase(i);
    }
    return taskPtr;
}


/* =============================================================================
 * populateParentQuery
 * -- Modifies contents of parentQueryVectorPtr
 * =============================================================================
 */
void
populateParentQueryVector (net_t* netPtr,
                           long id,
                           query_t* queries,
                           std::vector<query_t*>* parentQueryVectorPtr)
{
    parentQueryVectorPtr->clear();

    std::set<long>* parentIdListPtr = net_getParentIdListPtr(netPtr, id);
    for (auto it : *parentIdListPtr) {
        long parentId = it;
        vector_query_push(parentQueryVectorPtr, &queries[parentId]);
    }
}


/* =============================================================================
 * TMpopulateParentQueryVector
 * -- Modifies contents of parentQueryVectorPtr
 * =============================================================================
 */
__attribute__((transaction_safe))
void
TMpopulateParentQueryVector (net_t* netPtr,
                             long id,
                             query_t* queries,
                             std::vector<query_t*>* parentQueryVectorPtr)
{
    parentQueryVectorPtr->clear();

    std::set<long>* parentIdListPtr = net_getParentIdListPtr(netPtr, id);
    for (auto it : *parentIdListPtr) {
        //long parentId = (long)TMLIST_ITER_NEXT(&it, parentIdListPtr);
        long parentId = it;
        vector_query_push(parentQueryVectorPtr, &queries[parentId]);
    }
}


/* =============================================================================
 * TMpopulateQueryVectors
 * -- Modifies contents of queryVectorPtr and parentQueryVectorPtr
 * =============================================================================
 */
__attribute__((transaction_safe))
void
TMpopulateQueryVectors (net_t* netPtr,
                        long id,
                        query_t* queries,
                        std::vector<query_t*>* queryVectorPtr,
                        std::vector<query_t*>* parentQueryVectorPtr)
{
    TMpopulateParentQueryVector(netPtr, id, queries, parentQueryVectorPtr);

    // copy parentQueryVectorPtr to queryVectorPtr
    // [TODO] if we passed references, we could use a copy constructor
    queryVectorPtr->clear();
    for (auto i : *parentQueryVectorPtr)
        vector_query_push(queryVectorPtr, i);

    // PVECTOR_COPY(queryVectorPtr, parentQueryVectorPtr);
    vector_query_push(queryVectorPtr, &queries[id]);
    compareQuery_t sorter;
    std::sort(queryVectorPtr->begin(), queryVectorPtr->end(), sorter);
    // vector_sort(queryVectorPtr, &compareQuery);
}


/* =============================================================================
 * computeLocalLogLikelihoodHelper
 * -- Recursive helper routine
 * =============================================================================
 */
__attribute__((transaction_safe))
float
computeLocalLogLikelihoodHelper (long i,
                                 long numParent,
                                 adtree_t* adtreePtr,
                                 query_t* queries,
                                 std::vector<query_t*>* queryVectorPtr,
                                 std::vector<query_t*>* parentQueryVectorPtr)
{
    if (i >= numParent) {
      //[wer] this function contains a log(), which was not __attribute__((transaction_safe))
        return computeSpecificLocalLogLikelihood(adtreePtr,
                                                 queryVectorPtr,
                                                 parentQueryVectorPtr);
    }

    float localLogLikelihood = 0.0;

    query_t* parentQueryPtr = parentQueryVectorPtr->at(i);
    long parentIndex = parentQueryPtr->index;

    queries[parentIndex].value = 0;
    localLogLikelihood += computeLocalLogLikelihoodHelper((i + 1),
                                                          numParent,
                                                          adtreePtr,
                                                          queries,
                                                          queryVectorPtr,
                                                          parentQueryVectorPtr);

    queries[parentIndex].value = 1;
    localLogLikelihood += computeLocalLogLikelihoodHelper((i + 1),
                                                          numParent,
                                                          adtreePtr,
                                                          queries,
                                                          queryVectorPtr,
                                                          parentQueryVectorPtr);

    queries[parentIndex].value = QUERY_VALUE_WILDCARD;

    return localLogLikelihood;
}


/* =============================================================================
 * computeLocalLogLikelihood
 * -- Populate the query vectors before passing as args
 * =============================================================================
 */
//TM_PURE
__attribute__((transaction_safe))
float
computeLocalLogLikelihood (long id,
                           adtree_t* adtreePtr,
                           query_t* queries,
                           std::vector<query_t*>* queryVectorPtr,
                           std::vector<query_t*>* parentQueryVectorPtr)
{
    long numParent = parentQueryVectorPtr->size();
    float localLogLikelihood = 0.0;

    queries[id].value = 0;
    localLogLikelihood += computeLocalLogLikelihoodHelper(0,
                                                          numParent,
                                                          adtreePtr,
                                                          queries,
                                                          queryVectorPtr,
                                                          parentQueryVectorPtr);

    queries[id].value = 1;
    localLogLikelihood += computeLocalLogLikelihoodHelper(0,
                                                          numParent,
                                                          adtreePtr,
                                                          queries,
                                                          queryVectorPtr,
                                                          parentQueryVectorPtr);

    queries[id].value = QUERY_VALUE_WILDCARD;


    return localLogLikelihood;
}


/* =============================================================================
 * TMfindBestInsertTask
 * =============================================================================
 */
__attribute__((transaction_safe))
void
TMfindBestInsertTask (learner_task_t * dest,  findBestTaskArg_t* argPtr)
{
    long       toId                     = argPtr->toId;
    learner_t* learnerPtr               = argPtr->learnerPtr;
    query_t*   queries                  = argPtr->queries;
    std::vector<query_t*>*  queryVectorPtr           = argPtr->queryVectorPtr;
    std::vector<query_t*>*  parentQueryVectorPtr     = argPtr->parentQueryVectorPtr;
    long       numTotalParent           = argPtr->numTotalParent;
    float      basePenalty              = argPtr->basePenalty;
    float      baseLogLikelihood        = argPtr->baseLogLikelihood;
    std::vector<bool>*  invalidBitmapPtr         = argPtr->bitmapPtr;
    std::queue<long>*   workQueuePtr             = argPtr->workQueuePtr;
    std::vector<query_t*>*  baseParentQueryVectorPtr = argPtr->aQueryVectorPtr;
    std::vector<query_t*>*  baseQueryVectorPtr       = argPtr->bQueryVectorPtr;

    bool status;
    adtree_t* adtreePtr               = learnerPtr->adtreePtr;
    net_t*    netPtr                  = learnerPtr->netPtr;
    float*    localBaseLogLikelihoods = learnerPtr->localBaseLogLikelihoods;

    //[wer] this function contained unsafe calls, fixed
    TMpopulateParentQueryVector(netPtr, toId, queries, parentQueryVectorPtr);

    /*
     * Create base query and parentQuery
     */
    // copy parentQueryVectorPtr into baseParentQueryVectorPtr and baseQueryVectorPtr
    // TODO: use copy constructors instead?
    baseParentQueryVectorPtr->clear();
    baseQueryVectorPtr->clear();
    for (auto i : *parentQueryVectorPtr) {
        vector_query_push(baseParentQueryVectorPtr, i);
        vector_query_push(baseQueryVectorPtr, i);
    }

    vector_query_push(baseQueryVectorPtr, &queries[toId]);

    //[wer] was TM_PURE due to qsort(), now __attribute__((transaction_safe))
    compareQuery_t sorter;
    std::sort(queryVectorPtr->begin(), queryVectorPtr->end(), sorter);
    // vector_sort(queryVectorPtr, &compareQuery);

    /*
     * Search all possible valid operations for better local log likelihood
     */

    float bestFromId = toId; /* flag for not found */
    float oldLocalLogLikelihood = localBaseLogLikelihoods[toId];
    float bestLocalLogLikelihood = oldLocalLogLikelihood;

    // [wer] __attribute__((transaction_safe)) now
    status = TMnet_findDescendants(netPtr, toId, invalidBitmapPtr, workQueuePtr);

    assert(status);
    long fromId = -1;
    // __attribute__((transaction_safe))
    std::set<long>* parentIdListPtr = net_getParentIdListPtr(netPtr, toId);
    long maxNumEdgeLearned = global_maxNumEdgeLearned;

    // NB: bitmap_findClear is no longer available to us, so let's roll a
    // quick lambda to do the work
    auto findClear = [invalidBitmapPtr](long startIdx){
        long idx = -1;
        for (long a = startIdx; a < (long)invalidBitmapPtr->size(); ++a)
            if (!invalidBitmapPtr->at(a))
                return a;
        return idx;
    };

    if ((maxNumEdgeLearned < 0) || (parentIdListPtr->size() <= maxNumEdgeLearned)) {
        for (auto it : *parentIdListPtr) {
          //long parentId = (long)TMLIST_ITER_NEXT(&it, parentIdListPtr);
            long parentId = it;
            invalidBitmapPtr->at(parentId) = true; /* invalid since already have edge */
        }

        while ((fromId = findClear((fromId + 1))) >= 0) {
            if (fromId == toId) {
                continue;
            }

            //[wer] __attribute__((transaction_safe))
            // TODO: use copy constructor?
            queryVectorPtr->clear();
            for (auto i : *baseQueryVectorPtr)
                vector_query_push(queryVectorPtr, i);
            vector_query_push(queryVectorPtr, &queries[fromId]);
            //[wer] was TM_PURE due to qsort(), fixed
            compareQuery_t sorter;
            std::sort(queryVectorPtr->begin(), queryVectorPtr->end(), sorter);
            // vector_sort(queryVectorPtr, &compareQuery);

            // TODO: use copy constructor?
            parentQueryVectorPtr->clear();
            for (auto i : *baseParentQueryVectorPtr)
                vector_query_push(parentQueryVectorPtr, i);
            vector_query_push(parentQueryVectorPtr, &queries[fromId]);
            std::sort(parentQueryVectorPtr->begin(), parentQueryVectorPtr->end(), sorter);
            // vector_sort(parentQueryVectorPtr, &compareQuery);

            //[wer] in computeLocal...(), there's a function log(), which not __attribute__((transaction_safe))
            float newLocalLogLikelihood = computeLocalLogLikelihood(toId,
                                                                    adtreePtr,
                                                                    queries,
                                                                    queryVectorPtr,
                                                                    parentQueryVectorPtr);

            if (newLocalLogLikelihood > bestLocalLogLikelihood) {
                bestLocalLogLikelihood = newLocalLogLikelihood;
                bestFromId = fromId;
            }

        } /* foreach valid parent */

    } /* if have not exceeded max number of edges to learn */

    /*
     * Return best task; Note: if none is better, fromId will equal toId
     */
    learner_task_t bestTask;
    bestTask.op     = OPERATION_INSERT;
    bestTask.fromId = bestFromId;
    bestTask.toId   = toId;
    bestTask.score  = 0.0;

    if (bestFromId != toId) {
        long numRecord = adtreePtr->numRecord;
        long numParent = parentIdListPtr->size() + 1;
        float penalty =
            (numTotalParent + numParent * global_insertPenalty) * basePenalty;
        float logLikelihood = numRecord * (baseLogLikelihood +
                                           + bestLocalLogLikelihood
                                           - oldLocalLogLikelihood);
        float bestScore = penalty + logLikelihood;
        bestTask.score  = bestScore;
    }

    *dest = bestTask;
}


#ifdef LEARNER_TRY_REMOVE
/* =============================================================================
 * TMfindBestRemoveTask
 * =============================================================================
 */
__attribute__((transaction_safe))
//learner_task_t
void
TMfindBestRemoveTask (learner_task_t * dest,  findBestTaskArg_t* argPtr)
{
    long       toId                     = argPtr->toId;
    learner_t* learnerPtr               = argPtr->learnerPtr;
    query_t*   queries                  = argPtr->queries;
    std::vector<query_t*>*  queryVectorPtr           = argPtr->queryVectorPtr;
    std::vector<query_t*>*  parentQueryVectorPtr     = argPtr->parentQueryVectorPtr;
    long       numTotalParent           = argPtr->numTotalParent;
    float      basePenalty              = argPtr->basePenalty;
    float      baseLogLikelihood        = argPtr->baseLogLikelihood;
    std::vector<query_t*>*  origParentQueryVectorPtr = argPtr->aQueryVectorPtr;

    adtree_t* adtreePtr = learnerPtr->adtreePtr;
    net_t* netPtr = learnerPtr->netPtr;
    float* localBaseLogLikelihoods = learnerPtr->localBaseLogLikelihoods;

    TMpopulateParentQueryVector(netPtr, toId, queries, origParentQueryVectorPtr);
    long numParent = origParentQueryVectorPtr->size();

    /*
     * Search all possible valid operations for better local log likelihood
     */

    float bestFromId = toId; /* flag for not found */
    float oldLocalLogLikelihood = localBaseLogLikelihoods[toId];
    float bestLocalLogLikelihood = oldLocalLogLikelihood;

    long i;
    for (i = 0; i < numParent; i++) {

        query_t* queryPtr = origParentQueryVectorPtr->at(i);
        long fromId = queryPtr->index;

        /*
         * Create parent query (subset of parents since remove an edge)
         */

        parentQueryVectorPtr->clear();

        long p;
        for (p = 0; p < numParent; p++) {
            if (p != fromId) {
                query_t* queryPtr = origParentQueryVectorPtr->at(p);
                vector_query_push(parentQueryVectorPtr, &queries[queryPtr->index]);
            }
        } /* create new parent query */

        /*
         * Create query
         */

        // TODO: use copy constructor?
        queryVectorPtr->clear();
        for (auto i : *parentQueryVectorPtr)
            vector_query_push(queryVectorPtr, i);
        vector_query_push(queryVectorPtr, &queries[toId]);
        compareQuery_t sorter;
        std::sort(queryVectorPtr->begin(), queryVectorPtr->end(), sorter);
        // vector_sort(queryVectorPtr, &compareQuery);

        /*
         * See if removing parent is better
         */

        float newLocalLogLikelihood = computeLocalLogLikelihood(toId,
                                                                adtreePtr,
                                                                queries,
                                                                queryVectorPtr,
                                                                parentQueryVectorPtr);

        if (newLocalLogLikelihood > bestLocalLogLikelihood) {
            bestLocalLogLikelihood = newLocalLogLikelihood;
            bestFromId = fromId;
        }

    } /* for each parent */

    /*
     * Return best task; Note: if none is better, fromId will equal toId
     */

    learner_task_t bestTask;
    bestTask.op     = OPERATION_REMOVE;
    bestTask.fromId = bestFromId;
    bestTask.toId   = toId;
    bestTask.score  = 0.0;

    if (bestFromId != toId) {
        long numRecord = adtreePtr->numRecord;
        float penalty = (numTotalParent - 1) * basePenalty;
        float logLikelihood = numRecord * (baseLogLikelihood +
                                            + bestLocalLogLikelihood
                                            - oldLocalLogLikelihood);
        float bestScore = penalty + logLikelihood;
        bestTask.score  = bestScore;
    }

    //[wer210]
    dest->op = bestTask.op;
    dest->fromId = bestTask.fromId;
    dest->toId = bestTask.toId;
    dest->score = bestTask.score;
    //return bestTask;
}
#endif /* LEARNER_TRY_REMOVE */


#ifdef LEARNER_TRY_REVERSE
/* =============================================================================
 * TMfindBestReverseTask
 * =============================================================================
 */
__attribute__((transaction_safe))
void
TMfindBestReverseTask (learner_task_t * dest,  findBestTaskArg_t* argPtr)
{
    long       toId                         = argPtr->toId;
    learner_t* learnerPtr                   = argPtr->learnerPtr;
    query_t*   queries                      = argPtr->queries;
    std::vector<query_t*>*  queryVectorPtr               = argPtr->queryVectorPtr;
    std::vector<query_t*>*  parentQueryVectorPtr         = argPtr->parentQueryVectorPtr;
    long       numTotalParent               = argPtr->numTotalParent;
    float      basePenalty                  = argPtr->basePenalty;
    float      baseLogLikelihood            = argPtr->baseLogLikelihood;
    std::vector<bool>*  visitedBitmapPtr             = argPtr->bitmapPtr;
    std::queue<long>*   workQueuePtr                 = argPtr->workQueuePtr;
    std::vector<query_t*>*  toOrigParentQueryVectorPtr   = argPtr->aQueryVectorPtr;
    std::vector<query_t*>*  fromOrigParentQueryVectorPtr = argPtr->bQueryVectorPtr;

    adtree_t* adtreePtr               = learnerPtr->adtreePtr;
    net_t*    netPtr                  = learnerPtr->netPtr;
    float*    localBaseLogLikelihoods = learnerPtr->localBaseLogLikelihoods;

    TMpopulateParentQueryVector(netPtr, toId, queries, toOrigParentQueryVectorPtr);
    long numParent = toOrigParentQueryVectorPtr->size();

    /*
     * Search all possible valid operations for better local log likelihood
     */

    long bestFromId = toId; /* flag for not found */
    float oldLocalLogLikelihood = (float)localBaseLogLikelihoods[toId];
    float bestLocalLogLikelihood = oldLocalLogLikelihood;
    long fromId = 0;

    long i;
    for (i = 0; i < numParent; i++) {
        query_t* queryPtr = toOrigParentQueryVectorPtr->at(i);
      fromId = queryPtr->index;

      bestLocalLogLikelihood = oldLocalLogLikelihood +
        (float)localBaseLogLikelihoods[fromId];

      //__attribute__((transaction_safe))
      TMpopulateParentQueryVector(netPtr,
                                  fromId,
                                  queries,
                                  fromOrigParentQueryVectorPtr);

      /*
       * Create parent query (subset of parents since remove an edge)
       */

      parentQueryVectorPtr->clear();

      long p;
      for (p = 0; p < numParent; p++) {
        if (p != fromId) {
            query_t* queryPtr = toOrigParentQueryVectorPtr->at(p);
            vector_query_push(parentQueryVectorPtr, &queries[queryPtr->index]);
        }
      } /* create new parent query */

        /*
         * Create query
         */

      // TODO: use copy constructor?
      queryVectorPtr->clear();
      for (auto i : *parentQueryVectorPtr)
          vector_query_push(queryVectorPtr, i);
      vector_query_push(queryVectorPtr, &queries[toId]);

      //[wer]__attribute__((transaction_safe))
      compareQuery_t sorter;
      std::sort(queryVectorPtr->begin(), queryVectorPtr->end(), sorter);
      // vector_sort(queryVectorPtr, &compareQuery);

      /*
       * Get log likelihood for removing parent from toId
       */

      float newLocalLogLikelihood = computeLocalLogLikelihood(toId,
                                                              adtreePtr,
                                                              queries,
                                                              queryVectorPtr,
                                                              parentQueryVectorPtr);


      /*
       * Get log likelihood for adding parent to fromId
       */

      // TODO: use copy ctor?
      parentQueryVectorPtr->clear();
      for (auto i : *fromOrigParentQueryVectorPtr)
          vector_query_push(parentQueryVectorPtr, i);
      vector_query_push(parentQueryVectorPtr, &queries[toId]);
      std::sort(parentQueryVectorPtr->begin(), parentQueryVectorPtr->end(), sorter);
      // vector_sort(parentQueryVectorPtr, &compareQuery);

      // TODO: use copy ctor?
      queryVectorPtr->clear();
      for (auto i : *parentQueryVectorPtr)
          vector_query_push(queryVectorPtr, i);
      vector_query_push(queryVectorPtr, &queries[fromId]);
      std::sort(queryVectorPtr->begin(), queryVectorPtr->end(), sorter);
      // vector_sort(queryVectorPtr, &compareQuery);

      newLocalLogLikelihood += computeLocalLogLikelihood(fromId,
                                                         adtreePtr,
                                                         queries,
                                                         queryVectorPtr,
                                                         parentQueryVectorPtr);
      /*
       * Record best
       */

      if (newLocalLogLikelihood > bestLocalLogLikelihood) {
        bestLocalLogLikelihood = newLocalLogLikelihood;
        bestFromId = fromId;
      }

    } /* for each parent */

    /*
     * Check validity of best
     */

    if (bestFromId != toId) {
      bool isTaskValid = true;
      TMnet_applyOperation(netPtr, OPERATION_REMOVE, bestFromId, toId);
      if (TMnet_isPath(netPtr, bestFromId, toId, visitedBitmapPtr,
                       workQueuePtr)) {
        isTaskValid = false;
      }

      TMnet_applyOperation(netPtr, OPERATION_INSERT, bestFromId, toId);
      if (!isTaskValid)
        bestFromId = toId;
    }

    /*
     * Return best task; Note: if none is better, fromId will equal toId
     */

    learner_task_t bestTask;
    bestTask.op     = OPERATION_REVERSE;
    bestTask.fromId = bestFromId;
    bestTask.toId   = toId;
    bestTask.score  = 0.0;

    if (bestFromId != toId) {
      float fromLocalLogLikelihood = localBaseLogLikelihoods[bestFromId];
      long numRecord = adtreePtr->numRecord;
      float penalty = numTotalParent * basePenalty;
      float logLikelihood = numRecord * (baseLogLikelihood +
                                         + bestLocalLogLikelihood
                                         - oldLocalLogLikelihood
                                         - fromLocalLogLikelihood);
      float bestScore = penalty + logLikelihood;
      bestTask.score  = bestScore;
    }

    *dest = bestTask;
}
#endif /* LEARNER_TRY_REVERSE */

/* =============================================================================
 * learnStructure
 *
 * Note it is okay if the score is not exact, as we are relaxing the greedy
 * search. This means we do not need to communicate baseLogLikelihood across
 * threads.
 * =============================================================================
 */
void
learnStructure (void* argPtr)
{
    learner_t* learnerPtr = (learner_t*)argPtr;
    net_t* netPtr = learnerPtr->netPtr;
    adtree_t* adtreePtr = learnerPtr->adtreePtr;
    long numRecord = adtreePtr->numRecord;
    float* localBaseLogLikelihoods = learnerPtr->localBaseLogLikelihoods;
    auto taskListPtr = learnerPtr->taskListPtr;

    float operationQualityFactor = global_operationQualityFactor;

    std::vector<bool>* visitedBitmapPtr = new std::vector<bool>(learnerPtr->adtreePtr->numVar);
    assert(visitedBitmapPtr);
    std::queue<long>* workQueuePtr = new std::queue<long>();
    assert(workQueuePtr);

    long numVar = adtreePtr->numVar;
    query_t* queries = (query_t*)malloc(numVar * sizeof(query_t));
    assert(queries);
    long v;
    for (v = 0; v < numVar; v++) {
        queries[v].index = v;
        queries[v].value = QUERY_VALUE_WILDCARD;
    }

    float basePenalty = (float)(-0.5 * log((double)numRecord));

    std::vector<query_t*>* queryVectorPtr = make_vector_query();
    assert(queryVectorPtr);
    std::vector<query_t*>* parentQueryVectorPtr = make_vector_query();
    assert(parentQueryVectorPtr);
    std::vector<query_t*>* aQueryVectorPtr = make_vector_query();
    assert(aQueryVectorPtr);
    std::vector<query_t*>* bQueryVectorPtr = make_vector_query();
    assert(bQueryVectorPtr);

    findBestTaskArg_t arg;
    arg.learnerPtr           = learnerPtr;
    arg.queries              = queries;
    arg.queryVectorPtr       = queryVectorPtr;
    arg.parentQueryVectorPtr = parentQueryVectorPtr;
    arg.bitmapPtr            = visitedBitmapPtr;
    arg.workQueuePtr         = workQueuePtr;
    arg.aQueryVectorPtr      = aQueryVectorPtr;
    arg.bQueryVectorPtr      = bQueryVectorPtr;


    while (1) {

        learner_task_t* taskPtr;
        __transaction_atomic {
          taskPtr = TMpopTask(  taskListPtr);
        }

        if (taskPtr == NULL) {
            break;
        }

        operation_t op = taskPtr->op;
        long fromId = taskPtr->fromId;
        long toId = taskPtr->toId;

        bool isTaskValid;

        __transaction_atomic {
        /*
         * Check if task is still valid
         */
        isTaskValid = true;

        switch (op) {
         case OPERATION_INSERT: {
                if (TMnet_hasEdge(netPtr, fromId, toId)
                    || TMnet_isPath(netPtr,
                                    toId,
                                    fromId,
                                    visitedBitmapPtr,
                                    workQueuePtr)
                   )
                {
                  isTaskValid = false;
                }
                break;
            }

         case OPERATION_REMOVE: {
                /* Can never create cycle, so always valid */
                break;
            }
         case OPERATION_REVERSE: {
                /* Temporarily remove edge for check */
                TMnet_applyOperation(netPtr, OPERATION_REMOVE, fromId, toId);
                if (TMnet_isPath(netPtr,
                                 fromId,
                                 toId,
                                 visitedBitmapPtr,
                                 workQueuePtr))
                {
                    isTaskValid = false;
                }
                TMnet_applyOperation(netPtr, OPERATION_INSERT, fromId, toId);
                break;
            }

         default:
          assert(0);
        }

#ifdef TEST_LEARNER
        printf("[task] op=%i from=%li to=%li score=%lf valid=%s\n",
               taskPtr->op, taskPtr->fromId, taskPtr->toId, taskPtr->score,
               (isTaskValid ? "yes" : "no"));
        fflush(stdout);
#endif

        /*
         * Perform task: update graph and probabilities
         */

        if (isTaskValid) {
          TMnet_applyOperation(netPtr, op, fromId, toId);
        }

        }
        float deltaLogLikelihood = 0.0;
        if (isTaskValid) {
            switch (op) {
                float newBaseLogLikelihood;
                case OPERATION_INSERT: {
                  __transaction_atomic {
                    TMpopulateQueryVectors(netPtr,
                                           toId,
                                           queries,
                                           queryVectorPtr,
                                           parentQueryVectorPtr);
                    newBaseLogLikelihood =
                      computeLocalLogLikelihood(toId,
                                                adtreePtr,
                                                queries,
                                                queryVectorPtr,
                                                parentQueryVectorPtr);
                    float toLocalBaseLogLikelihood = localBaseLogLikelihoods[toId];
                    deltaLogLikelihood +=
                        toLocalBaseLogLikelihood - newBaseLogLikelihood;
                    localBaseLogLikelihoods[toId] = newBaseLogLikelihood;
                  }

                  __transaction_atomic {
                    long numTotalParent = learnerPtr->numTotalParent;
                    learnerPtr->numTotalParent = (numTotalParent + 1);
                  }
                  break;
                }
#ifdef LEARNER_TRY_REMOVE
                case OPERATION_REMOVE: {
                  __transaction_atomic {
                    TMpopulateQueryVectors(netPtr,
                                           fromId,
                                           queries,
                                           queryVectorPtr,
                                           parentQueryVectorPtr);
                    newBaseLogLikelihood =
                        computeLocalLogLikelihood(fromId,
                                                  adtreePtr,
                                                  queries,
                                                  queryVectorPtr,
                                                  parentQueryVectorPtr);
                    float fromLocalBaseLogLikelihood = localBaseLogLikelihoods[fromId];
                    deltaLogLikelihood +=
                        fromLocalBaseLogLikelihood - newBaseLogLikelihood;
                    localBaseLogLikelihoods[fromId] = newBaseLogLikelihood;
                  }

                  __transaction_atomic {
                    long numTotalParent = learnerPtr->numTotalParent;
                    learnerPtr->numTotalParent = (numTotalParent - 1);
                  }
                  break;
                }
#endif /* LEARNER_TRY_REMOVE */
#ifdef LEARNER_TRY_REVERSE
                case OPERATION_REVERSE: {
                  __transaction_atomic {
                    TMpopulateQueryVectors(netPtr,
                                         fromId,
                                         queries,
                                         queryVectorPtr,
                                         parentQueryVectorPtr);
                    newBaseLogLikelihood =
                      computeLocalLogLikelihood(fromId,
                                                adtreePtr,
                                                queries,
                                                queryVectorPtr,
                                                parentQueryVectorPtr);
                    float fromLocalBaseLogLikelihood = localBaseLogLikelihoods[fromId];
                    deltaLogLikelihood +=
                      fromLocalBaseLogLikelihood - newBaseLogLikelihood;
                    localBaseLogLikelihoods[fromId] =  newBaseLogLikelihood;
                  }

                  __transaction_atomic {
                    TMpopulateQueryVectors(netPtr,
                                           toId,
                                           queries,
                                           queryVectorPtr,
                                           parentQueryVectorPtr);
                    newBaseLogLikelihood =
                      computeLocalLogLikelihood(toId,
                                                adtreePtr,
                                                queries,
                                                  queryVectorPtr,
                                                  parentQueryVectorPtr);
                    float toLocalBaseLogLikelihood = localBaseLogLikelihoods[toId];
                    deltaLogLikelihood +=
                        toLocalBaseLogLikelihood - newBaseLogLikelihood;
                    localBaseLogLikelihoods[toId] = newBaseLogLikelihood;
                  }
                  break;
                }
#endif /* LEARNER_TRY_REVERSE */
             default:
              assert(0);
            } /* switch op */

        } /* if isTaskValid */

        /*
         * Update/read globals
         */

        float baseLogLikelihood;
        long numTotalParent;

        __transaction_atomic {
          float oldBaseLogLikelihood = learnerPtr->baseLogLikelihood;
          float newBaseLogLikelihood = oldBaseLogLikelihood + deltaLogLikelihood;
          learnerPtr->baseLogLikelihood = newBaseLogLikelihood;
          baseLogLikelihood = newBaseLogLikelihood;
          numTotalParent = learnerPtr->numTotalParent;
        }

        /*
         * Find next task
         */

        float baseScore = ((float)numTotalParent * basePenalty)
                           + (numRecord * baseLogLikelihood);

        learner_task_t bestTask;
        bestTask.op     = NUM_OPERATION;
        bestTask.toId   = -1;
        bestTask.fromId = -1;
        bestTask.score  = baseScore;

        learner_task_t newTask;

        arg.toId              = toId;
        arg.numTotalParent    = numTotalParent;
        arg.basePenalty       = basePenalty;
        arg.baseLogLikelihood = baseLogLikelihood;

        __transaction_atomic {
          TMfindBestInsertTask(&newTask, &arg);
        }

        if ((newTask.fromId != newTask.toId) &&
            (newTask.score > (bestTask.score / operationQualityFactor)))
        {
          bestTask = newTask;
        }

#ifdef LEARNER_TRY_REMOVE
        __transaction_atomic {
          TMfindBestRemoveTask(&newTask, &arg);
        }

        if ((newTask.fromId != newTask.toId) &&
            (newTask.score > (bestTask.score / operationQualityFactor)))
        {
          bestTask = newTask;
        }

#endif /* LEARNER_TRY_REMOVE */

#ifdef LEARNER_TRY_REVERSE
        //[wer210] used to have problems, fixed(log, qsort)
        __transaction_atomic {
          TMfindBestReverseTask(&newTask, &arg);
        }

        if ((newTask.fromId != newTask.toId) &&
            (newTask.score > (bestTask.score / operationQualityFactor)))
        {
          bestTask = newTask;
        }

#endif /* LEARNER_TRY_REVERSE */

        if (bestTask.toId != -1) {
            learner_task_t* tasks = learnerPtr->tasks;
            tasks[toId] = bestTask;
            __transaction_atomic {
                taskListPtr->insert(&tasks[toId]);
            }

#ifdef TEST_LEARNER
            printf("[new]  op=%i from=%li to=%li score=%lf\n",
                   bestTask.op, bestTask.fromId, bestTask.toId, bestTask.score);
            fflush(stdout);
#endif
        }

    } /* while (tasks) */

    delete visitedBitmapPtr;
    delete workQueuePtr;
    delete bQueryVectorPtr;
    delete aQueryVectorPtr;
    delete queryVectorPtr;
    delete parentQueryVectorPtr;
    free(queries);
}


/* =============================================================================
 * learner_run
 * -- Call adtree_make before this
 * =============================================================================
 */
void
learner_run (learner_t* learnerPtr)
{
#ifdef OTM
#pragma omp parallel
    {
        createTaskList((void*)learnerPtr);
    }
#pragma omp parallel
    {
        learnStructure((void*)learnerPtr);
    }
#else
    thread_start(&createTaskList, (void*)learnerPtr);
    thread_start(&learnStructure, (void*)learnerPtr);
#endif
}


/* =============================================================================
 * learner_score
 * -- Score entire network
 * =============================================================================
 */
float
learner_score (learner_t* learnerPtr)
{
    adtree_t* adtreePtr = learnerPtr->adtreePtr;
    net_t* netPtr = learnerPtr->netPtr;

    std::vector<query_t*>* queryVectorPtr = make_vector_query();
    assert(queryVectorPtr);
    std::vector<query_t*>* parentQueryVectorPtr = make_vector_query();
    assert(parentQueryVectorPtr);

    long numVar = adtreePtr->numVar;
    query_t* queries = (query_t*)malloc(numVar * sizeof(query_t));
    assert(queries);
    long v;
    for (v = 0; v < numVar; v++) {
        queries[v].index = v;
        queries[v].value = QUERY_VALUE_WILDCARD;
    }

    long numTotalParent = 0;
    float logLikelihood = 0.0;

    for (v = 0; v < numVar; v++) {

        std::set<long>* parentIdListPtr = net_getParentIdListPtr(netPtr, v);
        numTotalParent += parentIdListPtr->size();


        TMpopulateQueryVectors(netPtr,
                               v,
                               queries,
                               queryVectorPtr,
                               parentQueryVectorPtr);
        float localLogLikelihood = computeLocalLogLikelihood(v,
                                                             adtreePtr,
                                                             queries,
                                                             queryVectorPtr,
                                                             parentQueryVectorPtr);
        logLikelihood += localLogLikelihood;
    }

    delete queryVectorPtr;
    delete parentQueryVectorPtr;
    free(queries);

    long numRecord = adtreePtr->numRecord;
    float penalty = (float)(-0.5 * (double)numTotalParent * log((double)numRecord));
    float score = penalty + numRecord * logLikelihood;

    return score;
}


/* #############################################################################
 * TEST_LEARNER
 * #############################################################################
 */
#ifdef TEST_LEARNER

#include <stdio.h>


void
testPartition (long min, long max, long n)
{
    long start;
    long stop;

    printf("min=%li max=%li, n=%li\n", min, max, n);

    long i;
    for (i = 0; i < n; i++) {
        createPartition(min, max, i, n, &start, &stop);
        printf("%li: %li -> %li\n", i, start, stop);
    }
    puts("");
}


int
main (int argc, char* argv[])
{
    thread_startup(1);

    puts("Starting...");

    testPartition(0, 4, 8);
    testPartition(0, 15, 8);
    testPartition(3, 103, 7);

    long numVar = 56;
    long numRecord = 256;

    random_t* randomPtr = random_alloc();
    data_t* dataPtr = data_alloc(numVar, numRecord, randomPtr);
    assert(dataPtr);
    data_generate(dataPtr, 0, 10, 10);

    adtree_t* adtreePtr = adtree_alloc();
    assert(adtreePtr);
    adtree_make(adtreePtr, dataPtr);


    learner_t* learnerPtr = learner_alloc(dataPtr, adtreePtr);
    assert(learnerPtr);

    data_free(dataPtr);

    learner_run(learnerPtr);

    assert(!net_isCycle(learnerPtr->netPtr));

    float score = learner_score(learnerPtr);
    printf("score = %lf\n", score);

    learner_free(learnerPtr);

    puts("Done.");

    adtree_free(adtreePtr);
    random_free(randomPtr);

    thread_shutdown();

    return 0;
}

#endif /* TEST_LEARNER */
