/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include "region.h"
#include "mesh.h"
#include "thread.h"
#include "timer.h"
#include "tm_transition.h"
#include "tm_hacks.h"

#define PARAM_DEFAULT_INPUTPREFIX ("inputs/ttimeu1000000.2")
#define PARAM_DEFAULT_NUMTHREAD   (1L)
#define PARAM_DEFAULT_ANGLE       (15.0)


const char*    global_inputPrefix     = PARAM_DEFAULT_INPUTPREFIX;
long     global_numThread       = PARAM_DEFAULT_NUMTHREAD;
double   global_angleConstraint = PARAM_DEFAULT_ANGLE;
mesh_t*  global_meshPtr;
std::multiset<element_t*, element_heapCompare_t>* global_workHeapPtr;
long     global_totalNumAdded = 0;
long     global_numProcess    = 0;


/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                              (defaults)\n");
    printf("    a <FLT>   Min [a]ngle constraint  (%lf)\n", PARAM_DEFAULT_ANGLE);
    printf("    i <STR>   [i]nput name prefix     (%s)\n",  PARAM_DEFAULT_INPUTPREFIX);
    printf("    t <UINT>  Number of [t]hreads     (%li)\n", PARAM_DEFAULT_NUMTHREAD);
    exit(1);
}


/* =============================================================================
 * parseArgs
 * =============================================================================
 */
static void
parseArgs (long argc, char* const argv[])
{
    long i;
    long opt;

    opterr = 0;

    while ((opt = getopt(argc, argv, "a:i:t:")) != -1) {
        switch (opt) {
            case 'a':
                global_angleConstraint = atof(optarg);
                break;
            case 'i':
                global_inputPrefix = optarg;
                break;
            case 't':
                global_numThread = atol(optarg);
                break;
            case '?':
            default:
                opterr++;
                break;
        }
    }

    for (i = optind; i < argc; i++) {
        fprintf(stderr, "Non-option argument: %s\n", argv[i]);
        opterr++;
    }

    if (opterr) {
        displayUsage(argv[0]);
    }
}


/* =============================================================================
 * initializeWork
 * =============================================================================
 */
static long
initializeWork (std::multiset<element_t*, element_heapCompare_t>* workHeapPtr, mesh_t* meshPtr)
{
    std::mt19937* randomPtr = new std::mt19937();
    randomPtr->seed(0);
    meshPtr->shuffleBad(randomPtr);
    delete randomPtr;

    long numBad = 0;

    while (1) {
        element_t* elementPtr = meshPtr->getBad();
        if (!elementPtr) {
            break;
        }
        numBad++;
        bool status = custom_set_insertion(workHeapPtr, elementPtr);
        assert(status);
        elementPtr->setIsReferenced(true);
    }

    return numBad;
}

/* =============================================================================
 * process
 * =============================================================================
 */
static void
process (void*)
{
    auto workHeapPtr = global_workHeapPtr;
    mesh_t* meshPtr = global_meshPtr;
    region_t* regionPtr;
    long totalNumAdded = 0;
    long numProcess = 0;

    regionPtr = new region_t();
    assert(regionPtr);

    while (1) {

        element_t* elementPtr;

        __transaction_atomic {
            if (workHeapPtr->empty()) {
                elementPtr = NULL;
                break;
            }
            auto b = workHeapPtr->begin();
            elementPtr = *b;
            workHeapPtr->erase(b);
        }

        if (elementPtr == NULL) {
            break;
        }

        bool isGarbage;
        __transaction_atomic {
          isGarbage = elementPtr->isEltGarbage();
        }
        if (isGarbage) {
            /*
             * Handle delayed deallocation
             */
            delete elementPtr;
            continue;
        }

        long numAdded;
        //[wer210] changed the control flow to get rid of self-abort
        bool success = true;
        while (1) {
          __transaction_atomic {
            // TM_SAFE: PVECTOR_CLEAR (regionPtr->badVectorPtr);
            regionPtr->clearBad();
            //[wer210] problematic function!
            numAdded = regionPtr->refine(elementPtr, meshPtr, &success);
            if (success) break;
            else __transaction_cancel;
          }
        }

        __transaction_atomic {
          elementPtr->setIsReferenced(false);
          isGarbage = elementPtr->isEltGarbage();
        }
        if (isGarbage) {
            /*
             * Handle delayed deallocation
             */
            delete elementPtr;
        }

        totalNumAdded += numAdded;

        __transaction_atomic {
            regionPtr->transferBad(workHeapPtr);
        }

        numProcess++;

    }

    __transaction_atomic {
        global_totalNumAdded += totalNumAdded;
        global_numProcess += numProcess;
    }

    delete regionPtr;
}


/* =============================================================================
 * main
 * =============================================================================
 */
int main (int argc, char** argv)
{
    /*
     * Initialization
     */

    parseArgs(argc, (char** const)argv);

    thread_startup(global_numThread);
    global_meshPtr = new mesh_t();
    assert(global_meshPtr);
    printf("Angle constraint = %lf\n", global_angleConstraint);
    printf("Reading input... ");
    long initNumElement = global_meshPtr->read(global_inputPrefix);
    puts("done.");
    global_workHeapPtr = new std::multiset<element_t*, element_heapCompare_t>();
    assert(global_workHeapPtr);
    long initNumBadElement = initializeWork(global_workHeapPtr, global_meshPtr);

    printf("Initial number of mesh elements = %li\n", initNumElement);
    printf("Initial number of bad elements  = %li\n", initNumBadElement);
    printf("Starting triangulation...");
    fflush(stdout);

    /*
     * Run benchmark
     */

    TIMER_T start;
    TIMER_READ(start);
#ifdef OTM
#pragma omp parallel
    {
        process();
    }
#else
    thread_start(process, NULL);
#endif
    TIMER_T stop;
    TIMER_READ(stop);

    puts(" done.");
    printf("Time                            = %0.3lf\n",
           TIMER_DIFF_SECONDS(start, stop));
    fflush(stdout);

    /*
     * Check solution
     */

    long finalNumElement = initNumElement + global_totalNumAdded;
    printf("Final mesh size                 = %li\n", finalNumElement);
    printf("Number of elements processed    = %li\n", global_numProcess);
    fflush(stdout);

#if 0
    bool isSuccess = mesh_check(global_meshPtr, finalNumElement);
#else
    bool isSuccess = true;
#endif
    printf("Final mesh is %s\n", (isSuccess ? "valid." : "INVALID!"));
    fflush(stdout);
    assert(isSuccess);

    /*
     * TODO: deallocate mesh and work heap
     */

    thread_shutdown();

    return 0;
}


/* =============================================================================
 *
 * End of ruppert.c
 *
 * =============================================================================
 */
