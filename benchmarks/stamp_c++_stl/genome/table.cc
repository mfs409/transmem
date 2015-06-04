/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * table.c: Fixed-size hash table
 */

#include <cassert>
#include <cstdlib>
#include "table.h"

/* =============================================================================
 * table_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
table_t::table_t(long _numBucket)
{
    numBucket = _numBucket;
    buckets = new std::set<constructEntry_t*>*[numBucket];
    assert(buckets != NULL);

    for (long i = 0; i < numBucket; i++) {
        buckets[i] = new std::set<constructEntry_t*>();
        assert(buckets[i]);
    }
}


/* =============================================================================
 * table_insert
 * -- Returns true if successful, else false
 * =============================================================================
 */
__attribute__((transaction_safe))
bool table_t::insert(unsigned long hash, constructEntry_t* dataPtr)
{
    long i = hash % numBucket;
    auto res = buckets[i]->insert(dataPtr);
    return res.second;
}



/* =============================================================================
 * table_free
 * =============================================================================
 */
table_t::~table_t()
{
    /* TODO: fix mixed sequential/parallel allocation */
    for (long i = 0; i < numBucket; i++) {
        delete (buckets[i]);
    }

    free(buckets);
}


/* =============================================================================
 * TEST_TABLE
 * =============================================================================
 */
#ifdef TEST_TABLE


#include <stdio.h>


static void
printTable (table_t* tablePtr)
{
    long i;

    for (i = 0; i < tablePtr->numBucket; i++) {
        list_iter_t it;
        printf("%2i: [", i);
        list_iter_reset(&it, tablePtr->buckets[i]);
        while (list_iter_hasNext(&it, tablePtr->buckets[i])) {
            printf("%li ", *(long*)list_iter_next(&it, tablePtr->buckets[i]));
        }
        puts("]");
    }
}


int
main ()
{
    table_t* tablePtr;
    long hash[] = {3, 1, 4, 1, 5, 9, 2, 6, 8, 7, -1};
    long i;

    bool status = memory_init(1, 4, 2);
    assert(status);

    puts("Starting...");

    tablePtr = table_alloc(8, NULL);

    for (i = 0; hash[i] >= 0; i++ ) {
        bool status = table_insert(tablePtr,
                                     (ulong_t)hash[i],
                                     (void*)&hash[i])
        assert(status);
        printTable(tablePtr);
        puts("");
    }

    for (i = 0; hash[i] >= 0; i++ ) {
        bool status = table_remove(tablePtr,
                                     (ulong_t)hash[i],
                                     (void*)&hash[i])
        assert(status);
        printTable(tablePtr);
        puts("");
    }

    table_free(tablePtr);

    puts("Done.");

    return 0;
}


#endif /* TEST_TABLE */


/* =============================================================================
 *
 * End of table.c
 *
 * =============================================================================
 */
