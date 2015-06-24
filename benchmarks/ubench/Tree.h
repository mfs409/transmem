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

/// The RBTree benchmark is a traditional test of TM performance and
/// correctness.
///
/// On the correctness side, RBTrees tend to exercise all of
/// the corner cases of a TM implementation (static read-only vs. dynamic
/// read-only vs. read-write; reads of a transaction's own writes; multiple
/// reads and writes of the same location; etc).
///
/// On the performance side, a RBTree should have enough concurrency, even
/// with a small element range (e.g., 256 unique keys), to show good
/// scalability.  The default test is 256 keys / 33% lookup.  This can be
/// moved as high as 20-bit keys and 90% lookup, to show highly disjoint
/// workloads.
class RBTree
{
    /// making this a bool probably wouldn't help us any.
    enum Color { RED, BLACK };

    /// Node of an RBTree
    struct RBNode
    {
        Color   m_color;    /// color (RED or BLACK)
        int     m_val;      /// value stored at this node
        RBNode* m_parent;   /// pointer to parent
        int     m_ID;       /// 0 or 1 to indicate if left or right child
        RBNode* m_child[2]; /// pointers to children

        /// basic constructor
        RBNode(Color color = BLACK,
               long val = -1,
               RBNode* parent = NULL,
               long ID = 0,
               RBNode* child0 = NULL,
               RBNode* child1 = NULL)
            : m_color(color), m_val(val), m_parent(parent), m_ID(ID)
        {
            m_child[0] = child0;
            m_child[1] = child1;
        }
    };

    /// helper functions for sanity checks
    static int blackHeight(const RBNode* x);
    static bool redViolation(const RBNode* p_r, const RBNode* x);
    static bool validParents(const RBNode* p, int xID, const RBNode* x);
    static bool inOrder(const RBNode* x, int lowerBound, int upperBound);

    RBNode* sentinel;

  public:

    RBTree();

    // standard IntSet methods
    __attribute__((transaction_safe))
    bool lookup(int val) const;
    __attribute__((transaction_safe))
    bool insert(int val);
    __attribute__((transaction_safe))
    bool remove(int val);
    bool isSane() const;

    // custom method that always modifies the tree
    __attribute__((transaction_safe))
    void modify(int val);

};
