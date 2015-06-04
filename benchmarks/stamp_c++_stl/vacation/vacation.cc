/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <getopt.h>
#include <random>
#include "client.h"
#include "manager.h"
#include "timer.h"
#include "thread.h"

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

pthread_barrier_t* global_barrierPtr;

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
static bool
addCustomer (manager_t* managerPtr, long id, long, long)
{
    return managerPtr->addCustomer(id);
}

/* Three quick-and-dirty functions so that we don't need method pointers in the following code */
bool manager_addCar(manager_t* m, long a, long b, long c) {
    return m->addCar(a, b, c);
}

bool manager_addFlight(manager_t* m, long a, long b, long c) {
    return m->addFlight(a, b, c);
}

bool manager_addRoom(manager_t* m, long a, long b, long c) {
    return m->addRoom(a, b, c);
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
    std::mt19937 randomPtr;
    long* ids;
    bool (*manager_add[])(manager_t*, long, long, long) = {
        &manager_addCar,
        &manager_addFlight,
        &manager_addRoom,
        &addCustomer
    };
    long t;
    long numTable = sizeof(manager_add) / sizeof(manager_add[0]);

    printf("Initializing manager... ");
    fflush(stdout);

    managerPtr = new manager_t();
    assert(managerPtr != NULL);

    numRelation = (long)global_params[PARAM_RELATIONS];
    ids = (long*)malloc(numRelation * sizeof(long));
    for (i = 0; i < numRelation; i++) {
        ids[i] = i + 1;
    }

    for (t = 0; t < numTable; t++) {

        /* Shuffle ids */
        for (i = 0; i < numRelation; i++) {
            long x = randomPtr() % numRelation;
            long y = randomPtr() % numRelation;
            long tmp = ids[x];
            ids[x] = ids[y];
            ids[y] = tmp;
        }

        /* Populate table */
        for (i = 0; i < numRelation; i++) {
            bool status;
            long id = ids[i];
            long num = ((randomPtr() % 5) + 1) * 100;
            long price = ((randomPtr() % 5) * 10) + 50;
            status = manager_add[t](managerPtr, id, num, price);
            assert(status);
        }

    } /* for t */

    puts("done.");
    fflush(stdout);

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

    clients = (client_t**)malloc(numClient * sizeof(client_t*));
    assert(clients != NULL);
    numTransactionPerClient = (long)((double)numTransaction / (double)numClient + 0.5);
    queryRange = (long)((double)percentQuery / 100.0 * (double)numRelation + 0.5);

    for (i = 0; i < numClient; i++) {
        clients[i] = new client_t(i,
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

    return clients;
}

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

    std::map<long, customer_t*>* custs = managerPtr->customerTable;
    std::map<long, reservation_t*>* tbls[] = {
        managerPtr->carTable,
        managerPtr->flightTable,
        managerPtr->roomTable,
    };

    long numTable = sizeof(tbls) / sizeof(tbls[0]);
    bool (*manager_add[])(manager_t*, long, long, long) = {
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
        auto f = custs->find(i);
        if (f->first == i) {
            auto r = custs->erase(i);
            if (r != 0) {
                auto c = custs->find(i);
                assert(c == custs->end());
            }
            else {
                assert(r == 0);
            }
        }
        else {
            assert(f == custs->end());
        }
    }

    /* Check reservation tables for consistency and unique ids */
    for (t = 0; t < numTable; t++) {
        for (i = 1; i <= numRelation; i++) {
            auto f = tbls[t]->find(i);
            if (f->first == i) {
                assert(manager_add[t](managerPtr, i, 0, 0)); /* validate entry */
                auto e = tbls[t]->erase(i);
                if (e == 1) {
                    auto k = tbls[t]->erase(i);
                    assert(k == 0);
                }
                else {
                    assert(e == 0);
                }
            }
            else {
                assert(f == tbls[t]->end());
            }
        }
    }

    puts("done.");
    fflush(stdout);
}

/* =============================================================================
 * freeClients
 * =============================================================================
 */
static void
freeClients (client_t** clients)
{
    long numClient = (long)global_params[PARAM_CLIENTS];

    for (long i = 0; i < numClient; i++) {
        client_t* clientPtr = clients[i];
        delete clientPtr;
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
    thread_start(client_run, (void*)clients);
    TIMER_READ(stop);
    puts("done.");
    printf("Time = %0.6lf\n",
           TIMER_DIFF_SECONDS(start, stop));
    fflush(stdout);
    checkTables(managerPtr);

    /* Clean up */
    printf("Deallocating memory... ");
    fflush(stdout);
    freeClients(clients);
    /*
     * TODO: The contents of the manager's table need to be deallocated.
     */
    delete managerPtr;
    puts("done.");
    fflush(stdout);

    thread_shutdown();

    return 0;
}
