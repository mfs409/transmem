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

// We construct other data structures from the List. In order to do their
// sanity checks correctly, we might need to pass in a validation function of
// this type
typedef bool (*verifier)(uint32_t, uint32_t);

/// The LinkedList benchmark is a traditional test of TM performance and
/// correctness.
///
/// On the correctness side, when a TM implementation is failing, this is a
/// useful benchmark.  It has many reads per transaction, but few writes, and
/// never does a read-after-write.  If RBTree fails and this passes, then
/// there's probably a bug in read-after-write.
///
/// Regarding performance, Lists don't scale well with TM, because there are
/// lots of unnecessary conflicts.  No TM algorithm can change that.
class List
{
    /// Node in a List
    struct Node
    {
        int m_val;     /// the value stored in this node
        Node* m_next;  /// pointer to next element

        /// ctors
        Node(int val = -1) : m_val(val), m_next() { }
        Node(int val, Node* next) : m_val(val), m_next(next) { }
    };

    Node* sentinel;    /// the head node is a dummy node

  public:

    List();

    // standard IntSet methods
    __attribute__((transaction_safe))
    bool lookup(int val) const;
    __attribute__((transaction_safe))
    bool insert(int val);
    __attribute__((transaction_safe))
    bool remove(int val);
    bool isSane() const;

    // make sure the list is in sorted order and for each node x, v(x,
    // verifier_param) is true.  This is useful when the List is used to
    // create a Hash Table
    bool extendedSanityCheck(verifier v, uint32_t param) const;

    // find max and min
    __attribute__((transaction_safe))
    int findmax() const;
    __attribute__((transaction_safe))
    int findmin() const;

    // overwrite all elements up to val
    __attribute__((transaction_safe))
    void overwrite(int val);
};
