/* =============================================================================
 *
 * vacation.c
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 *
 * ------------------------------------------------------------------------
 *
 * For the license of ssca2, please see ssca2/COPYRIGHT
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 *
 * ------------------------------------------------------------------------
 *
 * Unless otherwise noted, the following license applies to STAMP files:
 *
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "client.h"
#include "customer.h"
#include "list.h"
#include "manager.h"
#include "map.h"
#include "memory.h"
#include "operation.h"
#include "random.h"
#include "reservation.h"
#include "thread.h"
#include "timer.h"
#include "tm.h"
#include "types.h"
#include "utility.h"

enum param_types {
    PARAM_CLIENTS      = (unsigned char)'t',
    PARAM_NUMBER       = (unsigned char)'n',
    PARAM_QUERIES      = (unsigned char)'q',
    PARAM_RELATIONS    = (unsigned char)'r',
    PARAM_TRANSACTIONS = (unsigned char)'T',
    PARAM_USER         = (unsigned char)'u'
};

#define PARAM_DEFAULT_CLIENTS      (1)
#define PARAM_DEFAULT_NUMBER       (4)
#define PARAM_DEFAULT_QUERIES      (60)
#define PARAM_DEFAULT_RELATIONS    (1 << 20)
#define PARAM_DEFAULT_TRANSACTIONS (1 << 22)
#define PARAM_DEFAULT_USER         (90)

double global_params[256]; /* 256 = ascii limit */


/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                                             (defaults)\n");
    printf("    t <UINT>   Number of clien[t]s ([t]hreads)       (%i)\n",
           PARAM_DEFAULT_CLIENTS);
    printf("    n <UINT>   [n]umber of user queries/transaction  (%i)\n",
           PARAM_DEFAULT_NUMBER);
    printf("    q <UINT>   Percentage of relations [q]ueried     (%i)\n",
           PARAM_DEFAULT_QUERIES);
    printf("    r <UINT>   Number of possible [r]elations        (%i)\n",
           PARAM_DEFAULT_RELATIONS);
    printf("    T <UINT>   Number of [T]ransactions              (%i)\n",
           PARAM_DEFAULT_TRANSACTIONS);
    printf("    u <UINT>   Percentage of [u]ser transactions     (%i)\n",
           PARAM_DEFAULT_USER);
    exit(1);
}


/* =============================================================================
 * setDefaultParams
 * =============================================================================
 */
static void
setDefaultParams ()
{
    global_params[PARAM_CLIENTS]      = PARAM_DEFAULT_CLIENTS;
    global_params[PARAM_NUMBER]       = PARAM_DEFAULT_NUMBER;
    global_params[PARAM_QUERIES]      = PARAM_DEFAULT_QUERIES;
    global_params[PARAM_RELATIONS]    = PARAM_DEFAULT_RELATIONS;
    global_params[PARAM_TRANSACTIONS] = PARAM_DEFAULT_TRANSACTIONS;
    global_params[PARAM_USER]         = PARAM_DEFAULT_USER;
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

    while ((opt = getopt(argc, argv, "t:n:q:r:T:u:L")) != -1) {
        switch (opt) {
            case 'T':
            case 'n':
            case 'q':
            case 'r':
            case 't':
            case 'u':
                global_params[(unsigned char)opt] = atol(optarg);
                break;
            case 'L':
                global_params[PARAM_NUMBER] = 2;
                global_params[PARAM_QUERIES] = 90;
                global_params[PARAM_USER] = 98;
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
 * addCustomer
 * -- Wrapper function
 * =============================================================================
 */
static bool_t
addCustomer (manager_t* managerPtr, long id)
{
    return manager_addCustomer(managerPtr, id);
}


/* =============================================================================
 * initializeManager
 * =============================================================================
 */
static manager_t*
initializeManager ()
{
    manager_t* managerPtr;
    long i;
    long numRelation;
    random_t* randomPtr;
    long* ids;
    bool_t (*manager_add[])(manager_t*, long, long, long) = {
        &manager_addCar,
        &manager_addFlight,
        &manager_addRoom,
        &addCustomer
    };
    long t;
    long numTable = sizeof(manager_add) / sizeof(manager_add[0]);

    printf("Initializing manager... ");
    fflush(stdout);

    randomPtr = random_alloc();
    assert(randomPtr != NULL);

    managerPtr = manager_alloc();
    assert(managerPtr != NULL);

    numRelation = (long)global_params[PARAM_RELATIONS];
    ids = (long*)malloc(numRelation * sizeof(long));
    for (i = 0; i < numRelation; i++) {
        ids[i] = i + 1;
    }

    for (t = 0; t < numTable; t++) {

        /* Shuffle ids */
        for (i = 0; i < numRelation; i++) {
            long x = random_generate(randomPtr) % numRelation;
            long y = random_generate(randomPtr) % numRelation;
            long tmp = ids[x];
            ids[x] = ids[y];
            ids[y] = tmp;
        }

        /* Populate table */
        for (i = 0; i < numRelation; i++) {
            bool_t status;
            long id = ids[i];
            long num = ((random_generate(randomPtr) % 5) + 1) * 100;
            long price = ((random_generate(randomPtr) % 5) * 10) + 50;
            status = manager_add[t](managerPtr, id, num, price);
            assert(status);
        }

    } /* for t */

    puts("done.");
    fflush(stdout);

    random_free(randomPtr);
    free(ids);

    return managerPtr;
}


/* =============================================================================
 * initializeClients
 * =============================================================================
 */
static client_t**
initializeClients (manager_t* managerPtr)
{
    random_t* randomPtr;
    client_t** clients;
    long i;
    long numClient = (long)global_params[PARAM_CLIENTS];
    long numTransaction = (long)global_params[PARAM_TRANSACTIONS];
    long numTransactionPerClient;
    long numQueryPerTransaction = (long)global_params[PARAM_NUMBER];
    long numRelation = (long)global_params[PARAM_RELATIONS];
    long percentQuery = (long)global_params[PARAM_QUERIES];
    long queryRange;
    long percentUser = (long)global_params[PARAM_USER];

    printf("Initializing clients... ");
    fflush(stdout);

    randomPtr = random_alloc();
    assert(randomPtr != NULL);

    clients = (client_t**)malloc(numClient * sizeof(client_t*));
    assert(clients != NULL);
    numTransactionPerClient = (long)((double)numTransaction / (double)numClient + 0.5);
    queryRange = (long)((double)percentQuery / 100.0 * (double)numRelation + 0.5);

    for (i = 0; i < numClient; i++) {
        clients[i] = client_alloc(i,
                                  managerPtr,
                                  numTransactionPerClient,
                                  numQueryPerTransaction,
                                  queryRange,
                                  percentUser);
        assert(clients[i]  != NULL);
    }

    puts("done.");
    printf("    Transactions        = %li\n", numTransaction);
    printf("    Clients             = %li\n", numClient);
    printf("    Transactions/client = %li\n", numTransactionPerClient);
    printf("    Queries/transaction = %li\n", numQueryPerTransaction);
    printf("    Relations           = %li\n", numRelation);
    printf("    Query percent       = %li\n", percentQuery);
    printf("    Query range         = %li\n", queryRange);
    printf("    Percent user        = %li\n", percentUser);
    fflush(stdout);

    random_free(randomPtr);

    return clients;
}

#if 0
/* =============================================================================
 * checkTables
 * -- some simple checks (not comprehensive)
 * -- dependent on tasks generated for clients in initializeClients()
 * =============================================================================
 */
static void
checkTables (manager_t* managerPtr)
{
    long i;
    long numRelation = (long)global_params[PARAM_RELATIONS];
    MAP_T* customerTablePtr = managerPtr->customerTablePtr;
    MAP_T* tables[] = {
        managerPtr->carTablePtr,
        managerPtr->flightTablePtr,
        managerPtr->roomTablePtr,
    };
    long numTable = sizeof(tables) / sizeof(tables[0]);
    bool_t (*manager_add[])(manager_t*, long, long, long) = {
        &manager_addCar,
        &manager_addFlight,
        &manager_addRoom
    };
    long t;

    printf("Checking tables... ");
    fflush(stdout);

    /* Check for unique customer IDs */
    long percentQuery = (long)global_params[PARAM_QUERIES];
    long queryRange = (long)((double)percentQuery / 100.0 * (double)numRelation + 0.5);
    long maxCustomerId = queryRange + 1;
    for (i = 1; i <= maxCustomerId; i++) {
        if (MAP_FIND(customerTablePtr, i)) {
            if (MAP_REMOVE(customerTablePtr, i)) {
                assert(!MAP_FIND(customerTablePtr, i));
            }
        }
    }

    /* Check reservation tables for consistency and unique ids */
    for (t = 0; t < numTable; t++) {
        MAP_T* tablePtr = tables[t];
        for (i = 1; i <= numRelation; i++) {
            if (MAP_FIND(tablePtr, i)) {
                assert(manager_add[t](managerPtr, i, 0, 0)); /* validate entry */
                if (MAP_REMOVE(tablePtr, i)) {
                    assert(!MAP_REMOVE(tablePtr, i));
                }
            }
        }
    }

    puts("done.");
    fflush(stdout);
}
#endif

/* =============================================================================
 * freeClients
 * =============================================================================
 */
static void
freeClients (client_t** clients)
{
    long i;
    long numClient = (long)global_params[PARAM_CLIENTS];

    for (i = 0; i < numClient; i++) {
        client_t* clientPtr = clients[i];
        client_free(clientPtr);
    }
}


/* =============================================================================
 * main
 * =============================================================================
 */
int main (int argc, char** argv)
{
    manager_t* managerPtr;
    client_t** clients;
    TIMER_T start;
    TIMER_T stop;

    /* Initialization */
    parseArgs(argc, (char** const)argv);
    managerPtr = initializeManager();
    assert(managerPtr != NULL);
    clients = initializeClients(managerPtr);
    assert(clients != NULL);
    long numThread = global_params[PARAM_CLIENTS];
    thread_startup(numThread);

    /* Run transactions */
    printf("Running clients... ");
    fflush(stdout);
    TIMER_READ(start);
#ifdef OTM
#pragma omp parallel
    {
        client_run(clients);
    }
#else
    thread_start(client_run, (void*)clients);
#endif
    TIMER_READ(stop);
    puts("done.");
    printf("Time = %0.6lf\n",
           TIMER_DIFF_SECONDS(start, stop));
    fflush(stdout);
    //[wer210] From here, no transactions anymore, however the following function
    //         call includes PLUS, which is incorrect unless manually clone
    //         a set of functions that does not contains PLUS specifically for it.
    //         However it does not affect total running time of transactions, so just
    //         comment it for now.
    //checkTables(managerPtr);

    /* Clean up */
    printf("Deallocating memory... ");
    fflush(stdout);
    freeClients(clients);
    /*
     * TODO: The contents of the manager's table need to be deallocated.
     */
    manager_free(managerPtr);
    puts("done.");
    fflush(stdout);

    thread_shutdown();

    return 0;
}


/* =============================================================================
 *
 * End of vacation.c
 *
 * =============================================================================
 */
