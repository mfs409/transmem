// -*-c++-*-

/**
 *  Copyright (C) 2015
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#pragma once

#include <set>

/// The StdSet benchmark is similar to RBTree, except it uses the C++
/// std::set object instead of a custom RBTree.
class StdSet
{
    std::set<int> s;

  public:

    StdSet() { }

    // standard IntSet methods
    __attribute__((transaction_safe))
    bool lookup(int val) const {
        return s.find(val) != s.end();
    }
    __attribute__((transaction_safe))
    bool insert(int val) {
        return s.insert(val).second;
    }
    __attribute__((transaction_safe))
    bool remove(int val) {
        return s.erase(val) == 1;
    }
    // NB: no sanity check... we can't see inside std::set
    bool isSane() const {
        return true;
    }

    // custom method that always modifies the tree
    __attribute__((transaction_safe))
    void modify(int val) {
        if (s.find(val) != s.end())
            s.erase(val);
        else
            s.insert(val);
    }

};
