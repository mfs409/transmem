/*
 *    Implements dynamic task queues to provide load balancing
 *        Sanjeev Kumar --- December, 2004
 */


#include <time.h>
#include "config.h"

#ifdef ENABLE_PTHREADS
#include "alamere.h"
pthread_t _M4_threadsTable[MAX_THREADS];
int _M4_threadsTableAllocated[MAX_THREADS];
pthread_mutexattr_t _M4_normalMutexAttr;
#endif //ENABLE_PTHREADS

// [transmem] include tmcondvar support
#ifdef ENABLE_TM
# include <tmcondvar.h>
#endif

#include "taskQInternal.h"
#include "taskQList.h"

// [transmem] can't access volatiles from inside of transactions... rename to
//            be sure we don't accidentally access nontransactionally.
#ifdef ENABLE_TM
static long                   tmNumThreads;
static volatile long          numTaskQs, threadsPerTaskQ, maxTasks;
#else
static volatile long          numThreads, numTaskQs, threadsPerTaskQ, maxTasks;
#endif
static volatile int           nextQ = 0;  // Just a hint. Not protected by locks.
static volatile int           parallelRegion = 0;
static volatile int           noMoreTasks = 0;

#if defined( TASKQ_DIST_GRID)
#include "taskQDistGrid.h"
#elif defined( TASKQ_DIST_LIST)
#include "taskQDistList.h"
#elif defined( TASKQ_DIST_FIXED)
#include "taskQDistFixed.h"
#else
#error "Missing Definition"
#endif

typedef struct {
#ifdef ENABLE_PTHREADS
  // [transmem] no lock when we're using TM
# ifndef ENABLE_TM
    pthread_mutex_t lock;
# endif
#endif //ENABLE_PTHREADS
    long                   statEnqueued, statLocal, *statStolen;
    char                   padding1[CACHE_LINE_SIZE];
    TaskQDetails           q;
    char                   padding2[CACHE_LINE_SIZE];
} TaskQ;

typedef struct {
#ifdef ENABLE_PTHREADS
  // [transmem] no lock, and tmcondvars, when in TM mode
# ifdef ENABLE_TM
  tmcondvar_t* tmTaskAvail;
  tmcondvar_t* tmTasksDone;
# else
    pthread_mutex_t lock;
    pthread_cond_t taskAvail;
    pthread_cond_t tasksDone;
# endif
#endif //ENABLE_PTHREADS
    volatile long         threadCount;
} Sync;

TaskQ *taskQs;
Sync  sync;

#define MAX_STEAL 8
static inline int calculateNumSteal( int available) {
    int half = ( available == 1) ? 1 : available/2;
    return ( half > MAX_STEAL) ? MAX_STEAL : half;
}

#if defined( TASKQ_DIST_GRID)
#include "taskQDistGrid.c"
#elif defined( TASKQ_DIST_LIST)
#include "taskQDistList.c"
#elif defined( TASKQ_DIST_FIXED)
#include "taskQDistFixed.c"
#else
#error "Missing Definition"
#endif

static void waitForTasks( void) {
    TRACE;
#ifdef ENABLE_PTHREADS
    // [transmem] use TM, not locks
#ifdef ENABLE_TM
    __transaction_atomic {
      sync.threadCount++;
      if ( sync.threadCount == tmNumThreads)
        tmcondvar_broadcast(sync.tmTasksDone);;
      tmcondvar_wait(sync.tmTaskAvail);
    }
#else
    pthread_mutex_lock(&(sync.lock));;
    sync.threadCount++;
    if ( sync.threadCount == numThreads)
        pthread_cond_broadcast(&sync.tasksDone);;
    pthread_cond_wait(&sync.taskAvail,&sync.lock);  pthread_mutex_unlock(&sync.lock);;
#endif
#else
    sync.threadCount++;
#endif //ENABLE_PTHREADS
    TRACE;
}

static void signalTasks( void) {
    TRACE;
    if ( sync.threadCount == 0)    return;    // Unsafe
#ifdef ENABLE_PTHREADS
    // [transmem] use locks instead of TM
#ifdef ENABLE_TM
    __transaction_atomic {
      sync.threadCount = 0;
      tmcondvar_broadcast(sync.tmTaskAvail);;
      tmcondvar_broadcast(sync.tmTasksDone);;
    }
#else
    pthread_mutex_lock(&(sync.lock));;
    sync.threadCount = 0;
    pthread_cond_broadcast(&sync.taskAvail);;
    pthread_cond_broadcast(&sync.tasksDone);;
    pthread_mutex_unlock(&(sync.lock));;
#endif
#else
    sync.threadCount = 0;
#endif //ENABLE_PTHREADS
    TRACE;
}

static int waitForEnd( void) {
    int done;
    TRACE;
#ifdef ENABLE_PTHREADS
    // [transmem] use TM.  Control flow is a bit icky here, since it's an
    //            if() instead of a while() inside the critical section.
#ifdef ENABLE_TM
    int mode = 1;
    while (mode != 0) {
      __transaction_atomic {
        if (mode == 1) {
          sync.threadCount++;
          if ( sync.threadCount != tmNumThreads) {
            tmcondvar_wait(sync.tmTasksDone);;
            mode = 2;
          }
          else {
            done = (sync.threadCount == tmNumThreads);
            mode = 0;
          }
        }
        else {
          done = (sync.threadCount == tmNumThreads);
          mode = 0;
        }
      }
    }
#else
    pthread_mutex_lock(&(sync.lock));;
    sync.threadCount++;
    if ( sync.threadCount != numThreads)
        pthread_cond_wait(&sync.tasksDone,&sync.lock);;
    done = (sync.threadCount == numThreads);
    pthread_mutex_unlock(&(sync.lock));;
#endif
#else
    sync.threadCount++;
    done = (sync.threadCount == numThreads);
#endif //ENABLE_PTHREADS
    TRACE;

    return done;
}

static int doOwnTasks( long myThreadId, long myQ) {
    void *task[NUM_FIELDS];
    int executed = 0;

    TRACE;
    while ( getATaskFromHead( &taskQs[myQ], task)) {
        ( ( TaskQTask3)task[0])( myThreadId, task[1], task[2], task[3]);
        executed = 1;
    }
    TRACE;
    return executed;
}

static int stealTasks( long myThreadId, long myQ) {
    int i, stolen = 0;

    TRACE;
    i = myQ + 1;
    while ( 1 ) {
        if ( i == numTaskQs)    i = 0;
        if ( i == myQ)    break;

        stolen = stealTasksSpecialized( &taskQs[myQ], &taskQs[i]);
        if( stolen) {
            IF_STATS(  {
#ifdef ENABLE_PTHREADS
// [transmem] use TM
#ifdef ENABLE_TM
                __transaction_atomic {
                  taskQs[myQ].statStolen[i] += stolen;
                }
#else
                pthread_mutex_lock(&(taskQs[myQ].lock));;
                taskQs[myQ].statStolen[i] += stolen;
                pthread_mutex_unlock(&(taskQs[myQ].lock));;
#endif
#else
                taskQs[myQ].statStolen[i] += stolen;
#endif //ENABLE_PTHREADS
            });
            break;
        } else {
            i++;
        }
    }
    TRACE;
    return stolen;
}

static void *taskQIdleLoop( void *arg) {
  // [transmem] configure the thread's condvar context
#ifdef ENABLE_TM
  tmcondvar_thread_init();
#endif
    long index = ( long)arg;
    long myQ = index / threadsPerTaskQ;
    int i = 0;
    int stolen;

    while ( 1 ) {
        waitForTasks();
        if ( noMoreTasks)    return 0;
        doOwnTasks( index, myQ);
        while ( 1) {
            stolen = stealTasks( index, myQ);
            if ( stolen)
                doOwnTasks( index, myQ);
            else
                break;
        }
        i++;
    }
}

void taskQInit( int numOfThreads, int maxNumOfTasks) {
    int i;

#ifdef ENABLE_PTHREADS
    ALAMERE_INIT(numOfThreads);
    ALAMERE_AFTER_CHECKPOINT();
    pthread_mutexattr_init( &_M4_normalMutexAttr);
    pthread_mutexattr_settype( &_M4_normalMutexAttr, PTHREAD_MUTEX_NORMAL);
    {
        int _M4_i;
        for ( _M4_i = 0; _M4_i < MAX_THREADS; _M4_i++) {
            _M4_threadsTableAllocated[_M4_i] = 0;
        }
    }
#endif //ENABLE_PTHREADS
;

    maxTasks = maxNumOfTasks;
    // [transmem] tmNumThreads isn't volatile... to be safe, always use a transaction
#ifdef ENABLE_TM
    __transaction_atomic { tmNumThreads = numOfThreads; }
#else
    numThreads = numOfThreads;
#endif
    threadsPerTaskQ = taskQGetParam( TaskQThreadsPerQueue);
    // [transmem] skip assertion when TM is on, instead of dealing with
    //            assertions that read transactional variables.
#ifndef ENABLE_TM
    DEBUG_ASSERT( ( numThreads >= 1) && ( threadsPerTaskQ >= 1));
#endif

    numTaskQs = (numOfThreads+threadsPerTaskQ-1)/threadsPerTaskQ;

    /*
    printf( "\n\n\t#####  Running TaskQ version Distributed %-15s with %ld threads and %ld queues  #####\n",
            VERSION,  numThreads, numTaskQs);
    printf( "\t##### \t\t\t\t\t\t[ built on %s at %s ]  #####\n\n", __DATE__, __TIME__);
    printf( "\t\t TaskQ mutex address                 :  %ld\n", ( long)&sync.lock);
    printf( "\t\t TaskQ condition variable 1 address  :  %ld\n", ( long)&sync.taskAvail);
    printf( "\t\t TaskQ condition variable 2 address  :  %ld\n", ( long)&sync.tasksDone);
    printf( "\n\n");
    */
    DEBUG_ANNOUNCE;

    taskQs = ( TaskQ *)malloc( sizeof( TaskQ) * numTaskQs);
    for ( i = 0; i < numTaskQs; i++) {
#ifdef ENABLE_PTHREADS
      // [transmem] no lock to configure
#ifndef ENABLE_TM
        pthread_mutex_init(&(taskQs[i].lock), NULL);;
#endif
#endif //ENABLE_PTHREADS
        taskQs[i].statStolen = ( long *)malloc( sizeof(long) * numTaskQs);

        initTaskQDetails( &taskQs[i]);
    }
    taskQResetStats();

#ifdef ENABLE_PTHREADS
    // [transmem] TM initialization instead of pthread
#ifdef ENABLE_TM
    sync.tmTaskAvail = tmcondvar_create();
    sync.tmTasksDone = tmcondvar_create();
#else
    pthread_mutex_init(&(sync.lock), NULL);;
    pthread_cond_init(&sync.taskAvail,NULL);;
    pthread_cond_init(&sync.tasksDone,NULL);;
#endif
#endif //ENABLE_PTHREADS
    sync.threadCount = 0;

#ifdef ENABLE_PTHREADS
    // [transmem] Must read tmNumThreads in a transaction, since it's not volatile
#ifdef ENABLE_TM
    long numThreads = 0;
    __transaction_atomic { numThreads = tmNumThreads; }
#endif
    for ( i = 1; i < numThreads; i++)
    {
        int _M4_i;
        for ( _M4_i = 0; _M4_i < MAX_THREADS; _M4_i++) {
            if ( _M4_threadsTableAllocated[_M4_i] == 0)    break;
        }
        pthread_create(&_M4_threadsTable[_M4_i],NULL,(void *(*)(void *))taskQIdleLoop,(void *)( long)i);
        _M4_threadsTableAllocated[_M4_i] = 1;
    }
#endif //ENABLE_PTHREADS
;

    waitForEnd();
}

static inline int pickQueue( int threadId) { // Needs work
    if ( parallelRegion)    return threadId/threadsPerTaskQ;
    int q = nextQ;
    int p = q+1;
    nextQ = ( p >= numTaskQs) ? 0 : p;
    DEBUG_ASSERT( q < numTaskQs);
    return q;
}

void taskQEnqueueGrid( TaskQTask taskFunction, TaskQThreadId threadId, long numOfDimensions,
                       long dimensionSize[MAX_DIMENSION], long tileSize[MAX_DIMENSION]) {
    taskQEnqueueGridSpecialized( ( TaskQTask3)taskFunction, threadId, numOfDimensions, tileSize);
    enqueueGridHelper( assignTasks, ( TaskQTask3)taskFunction, numOfDimensions, numTaskQs, dimensionSize, tileSize);
    if ( parallelRegion)    signalTasks();
}


static inline void taskQEnqueueTaskHelper( int threadId, void *task[NUM_FIELDS]) {
    TRACE;
    int queueNo = pickQueue( threadId);
    // if ( !parallelRegion)  printf( "%30ld %20ld\n", ( long)task[1], ( long)queueNo);
    taskQEnqueueTaskSpecialized( &taskQs[queueNo], task);
    if ( parallelRegion)    signalTasks();
}

void taskQEnqueueTask1( TaskQTask1 taskFunction, TaskQThreadId threadId, void *arg1) {
    TRACE;
    void *task[NUM_FIELDS];
    copyArgs1( task, taskFunction, arg1);
    taskQEnqueueTaskHelper( threadId, task);
}

void taskQEnqueueTask2( TaskQTask2 taskFunction, TaskQThreadId threadId, void *arg1, void *arg2) {
    TRACE;
    void *task[NUM_FIELDS];
    copyArgs2( task, taskFunction, arg1, arg2);
    taskQEnqueueTaskHelper( threadId, task);
}

void taskQEnqueueTask3( TaskQTask3 taskFunction, TaskQThreadId threadId, void *arg1, void *arg2, void *arg3) {
    TRACE;
    void *task[NUM_FIELDS];
    copyArgs3( task, taskFunction, arg1, arg2, arg3);
    taskQEnqueueTaskHelper( threadId, task);
}

void taskQWait( void) {
    static int i = 0;
    int done;
    parallelRegion = 1;
    signalTasks();
    TRACE;
    do {
        doOwnTasks( 0, 0);
        stealTasks( 0, 0);
        doOwnTasks( 0, 0);
        done = waitForEnd();
    } while (!done);
    parallelRegion = 0;
    TRACE;
    i++;
}

void taskQResetStats() {
    IF_STATS( {
        int i; int j;
        for ( i = 0; i < numTaskQs; i++) {
            taskQs[i].statEnqueued = 0;
            taskQs[i].statLocal = 0;
            for ( j = 0; j < numTaskQs; j++) {
                taskQs[i].statStolen[j] = 0;
            }
        }
    });
}

void taskQPrnStats() {
    IF_STATS( {
        long j; long i; long total1 = 0; long total2 = 0;
        printf( "\n\n\t#####  Cumulative statistics from Task Queues #####\n\n");
        printf( "\t\t%-10s    %-10s %-10s %-10s %-10s\n\n", "Queue", "Enqueued", "Local", "Stolen", "Executed");
        for ( j = 0; j < numTaskQs; j++) {
            long totalStolen = 0;
            for ( i = 0; i < numTaskQs; i++)    totalStolen += taskQs[j].statStolen[i];
            printf( "\t\t%5ld    %10ld %10ld %10ld %10ld\n", j, taskQs[j].statEnqueued, taskQs[j].statLocal-totalStolen, totalStolen, taskQs[j].statLocal);
            total1 += taskQs[j].statEnqueued;
            total2 += taskQs[j].statLocal;
        }
        printf( "\t\t%5s    %10ld %10s %10s %10ld\n", "Total", total1, "", "", total2);
        printf( "\n\n");

        printf( "\tBreakdown of Task Stealing\n\n\t%10s", "");
        for ( i = 0; i < numTaskQs; i++)  { printf( "%8ld Q", i); }
        printf( "\n\n");
        for ( j = 0; j < numTaskQs; j++) {
            printf( "\t%8ld T", j);
            for ( i = 0; i < numTaskQs; i++) {
                printf( "%10ld", taskQs[j].statStolen[i]);
            }
            printf( "\n");
        }
        printf( "\n\n");
    });
}

void taskQEnd( void) {
    noMoreTasks = 1;
    signalTasks();

#ifdef ENABLE_PTHREADS
    ALAMERE_END();;
#endif //ENABLE_PTHREADS
}
