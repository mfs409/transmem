/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <cassert>
#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include "maze.h"
#include "router.h"
#include "thread.h"
#include "timer.h"

enum param_types {
    PARAM_BENDCOST = (unsigned char)'b',
    PARAM_THREAD   = (unsigned char)'t',
    PARAM_XCOST    = (unsigned char)'x',
    PARAM_YCOST    = (unsigned char)'y',
    PARAM_ZCOST    = (unsigned char)'z'
};

enum param_defaults {
    PARAM_DEFAULT_BENDCOST = 1,
    PARAM_DEFAULT_THREAD   = 1,
    PARAM_DEFAULT_XCOST    = 1,
    PARAM_DEFAULT_YCOST    = 1,
    PARAM_DEFAULT_ZCOST    = 2
};

bool global_doPrint = false;
const char* global_inputFile = "inputs/random-x512-y512-z7-n512.txt";
long global_params[256]; /* 256 = ascii limit */


/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                            (defaults)\n");
    printf("    b <INT>    [b]end cost          (%i)\n", PARAM_DEFAULT_BENDCOST);
    printf("    i <FILE>   [i]nput file name    (%s)\n", global_inputFile);
    printf("    p          [p]rint routed maze  (false)\n");
    printf("    t <UINT>   Number of [t]hreads  (%i)\n", PARAM_DEFAULT_THREAD);
    printf("    x <UINT>   [x] movement cost    (%i)\n", PARAM_DEFAULT_XCOST);
    printf("    y <UINT>   [y] movement cost    (%i)\n", PARAM_DEFAULT_YCOST);
    printf("    z <UINT>   [z] movement cost    (%i)\n", PARAM_DEFAULT_ZCOST);
    exit(1);
}


/* =============================================================================
 * setDefaultParams
 * =============================================================================
 */
static void
setDefaultParams ()
{
    global_params[PARAM_BENDCOST] = PARAM_DEFAULT_BENDCOST;
    global_params[PARAM_THREAD]   = PARAM_DEFAULT_THREAD;
    global_params[PARAM_XCOST]    = PARAM_DEFAULT_XCOST;
    global_params[PARAM_YCOST]    = PARAM_DEFAULT_YCOST;
    global_params[PARAM_ZCOST]    = PARAM_DEFAULT_ZCOST;
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

    setDefaultParams();

    while ((opt = getopt(argc, argv, "b:i:pt:x:y:z:")) != -1) {
        switch (opt) {
            case 'b':
            case 't':
            case 'x':
            case 'y':
            case 'z':
                global_params[(unsigned char)opt] = atol(optarg);
                break;
            case 'i':
                global_inputFile = optarg;
                break;
            case 'p':
                global_doPrint = true;
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
 * main
 * =============================================================================
 */
int main (int argc, char** argv)
{

    /*
     * Initialization
     */
    parseArgs(argc, (char** const)argv);
    long numThread = global_params[PARAM_THREAD];
    thread_startup(numThread);
    maze_t* mazePtr = new maze_t();
    assert(mazePtr);
    long numPathToRoute = mazePtr->read(global_inputFile);
    router_t* routerPtr = new router_t(global_params[PARAM_XCOST],
                                       global_params[PARAM_YCOST],
                                       global_params[PARAM_ZCOST],
                                       global_params[PARAM_BENDCOST]);
    assert(routerPtr);
    std::set<std::vector<std::vector<long*>*>*>* pathVectorListPtr =
        new std::set<std::vector<std::vector<long*>*>*>();
    assert(pathVectorListPtr);

    /*
     * Run transactions
     */
    router_solve_arg_t routerArg = {routerPtr, mazePtr, pathVectorListPtr};
    TIMER_T startTime;
    TIMER_READ(startTime);
#ifdef OTM
#pragma omp parallel
    {
        router_solve((void *)&routerArg);
    }
#else
    thread_start(router_solve, (void*)&routerArg);
#endif

    TIMER_T stopTime;
    TIMER_READ(stopTime);

    long numPathRouted = 0;
    for (auto i : *pathVectorListPtr) {
        numPathRouted += i->size();
    }
    printf("Paths routed    = %li\n", numPathRouted);
    printf("Time            = %f\n", TIMER_DIFF_SECONDS(startTime, stopTime));

    /*
     * Check solution and clean up
     */
    assert(numPathRouted <= numPathToRoute);
    bool status = mazePtr->checkPaths(pathVectorListPtr, global_doPrint);
    assert(status == true);
    puts("Verification passed.");
    delete mazePtr;
    delete routerPtr;

    for (auto j : *pathVectorListPtr) {
        std::vector<std::vector<long*>*>* pathVectorPtr = j;
        for (auto i : *pathVectorPtr) {
            delete i;
        }
        delete pathVectorPtr;
    }
    delete pathVectorListPtr;

    thread_shutdown();


    return 0;
}


/* =============================================================================
 *
 * End of labyrinth.c
 *
 * =============================================================================
 */
