/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * table.h: Fixed-size hash table
 */

#pragma once

#include <set>
#include "types.h"

struct table_t {
    // [mfs] Ultimately this might need to become an std::unordered_map, but
    // for now we'll just make it an array of std::lists of constructEntry_t
    // list_t** buckets;
    std::set<constructEntry_t*>** buckets;
    long numBucket;

    table_t(long numBucket);

    /**
     * table_insert: Returns true if successful, else false
     */
    __attribute__((transaction_safe))
    bool insert(unsigned long hash, constructEntry_t* dataPtr);

    // NB: table_remove was not used, so we eliminated it

    ~table_t();
};
