// -*-c++-*-

/**
 *  Copyright (C) 2011, 2015
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#pragma once

#include <cstdlib>
#include <cstdint>

#include "List.h"

/// A HashBench should scale very well, since conflicts are exceedingly rare.
class HashTable
{
    /// The Hash class is an array of N_BUCKETS LinkedLists
    static const int N_BUCKETS = 256;

    /**
     *  during a sanity check, we want to make sure that every element in a
     *  bucket actually hashes to that bucket; we do it by passing this
     *  method to the extendedSanityCheck for the bucket.
     */
    static bool verify_hash_function(uint32_t val, uint32_t bucket) {
        return ((val % N_BUCKETS) == bucket);
    }

    /**
     *  Templated type defines what kind of list we'll use at each bucket.
     */
    List bucket[N_BUCKETS];

  public:

    // standard IntSet methods
    __attribute__((transaction_safe))
    bool insert(int val) {
        return bucket[val % N_BUCKETS].insert(val);
    }
    __attribute__((transaction_safe))
    bool lookup(int val) const {
        return bucket[val % N_BUCKETS].lookup(val);
    }
    __attribute__((transaction_safe))
    bool remove(int val) {
        return bucket[val % N_BUCKETS].remove(val);
    }
    bool isSane() const {
        for (int i = 0; i < N_BUCKETS; i++)
            if (!bucket[i].extendedSanityCheck(verify_hash_function, i))
                return false;
        return true;
    }
};
