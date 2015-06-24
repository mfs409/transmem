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
#include <cstdio>

/// The Counter benchmark is a degenerate IntSet benchmark.  We don't
/// actually support insert, lookup, and remove.  Instead, everything is just
/// an increment of the counter.
///
/// In truth, the only value of a counter benchmark is for debugging.  If a
/// TM implementation is crashing, this will help to determine whether writes
/// are even happening.
class Counter
{
    /// the integer counter upon which we operate
    int counter;

    /// a simple increment function
    int increment() {
        return ++counter;
    }

  public:

    /// Just zero the counter
    Counter() : counter(0) { }

    /// OK, this is gross, but we can just implement the three intset methods
    /// as calls to increment, so that we can re-use the existing benchmark
    /// harness code
    bool lookup(int val) {
        return increment() == 0;
    }
    bool insert(int val) {
        return increment() == 0;
    }
    bool remove(int val) {
        return increment() == 0;
    }

    /// isSane will just print the counter value, so we can eyeball it
    bool isSane() {
        printf("Counter value = %d\n", counter);
        return true;
    }
};
