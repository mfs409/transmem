/* =============================================================================
 *
 * rbtree.c
 * -- Red-black balanced binary search tree
 *
 * =============================================================================
 *
 * Copyright (C) Sun Microsystems Inc., 2006.  All Rights Reserved.
 * Authors: Dave Dice, Nir Shavit, Ori Shalev.
 *
 * STM: Transactional Locking for Disjoint Access Parallelism
 *
 * Transactional Locking II,
 * Dave Dice, Ori Shalev, Nir Shavit
 * DISC 2006, Sept 2006, Stockholm, Sweden.
 *
 * =============================================================================
 *
 * Modified by Chi Cao Minh, Aug 2006
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


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "memory.h"
#include "rbtree.h"
#include "tm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct node {
    void* k;
    void* v;
    struct node* p;
    struct node* l;
    struct node* r;
    long c;
} node_t;


struct rbtree {
    node_t* root;
    TM_SAFE long (*compare)(const void*, const void*);   /* returns {-1,0,1}, 0 -> equal */
};

#define LDA(a)              *(a)
#define STA(a,v)            *(a) = (v)
#define LDV(a)              (a)
#define STV(a,v)            (a) = (v)
#define LDF(o,f)            ((o)->f)
#define STF(o,f,v)          ((o)->f) = (v)
#define LDNODE(o,f)         ((node_t*)(LDF((o),f)))

#define TX_LDA(a)           TM_SHARED_READ_P(*(a))
#define TX_STA(a,v)         TM_SHARED_WRITE_P(*(a), v)
#define TX_LDV(a)           TM_SHARED_READ_P(a)
#define TX_STV(a,v)         TM_SHARED_WRITE_P(a, v)
#define TX_LDF(o,f)         ((long)TM_SHARED_READ((o)->f))
#define TX_LDF_P(o,f)       ((void*)TM_SHARED_READ_P((o)->f))
#define TX_STF(o,f,v)       TM_SHARED_WRITE((o)->f, v)
#define TX_STF_P(o,f,v)     TM_SHARED_WRITE_P((o)->f, v)
#define TX_LDNODE(o,f)      ((node_t*)(TX_LDF_P((o),f)))

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */

 TM_SAFE
node_t*
lookup (rbtree_t* s, void* k);

 TM_SAFE
void
TMrotateLeft (rbtree_t* s, node_t* x);

 TM_SAFE
void
TMrotateRight (rbtree_t* s, node_t* x);

 TM_SAFE
inline node_t*
TMparentOf (node_t* n);

 TM_SAFE
inline node_t*
TMleftOf (node_t* n);

 TM_SAFE
inline node_t*
TMrightOf (node_t* n);

 TM_SAFE
inline long
TMcolorOf (node_t* n);

 TM_SAFE
inline void
TMsetColor (node_t* n, long c);

 TM_SAFE
void
TMfixAfterInsertion (rbtree_t* s, node_t* x);

 TM_SAFE
node_t*
TMsuccessor  (node_t* t);

 TM_SAFE
void
TMfixAfterDeletion  (rbtree_t* s, node_t*  x);

 TM_SAFE
node_t*
TMinsert (rbtree_t* s, void* k, void* v, node_t* n);

 TM_SAFE
node_t*
getNode ();

 TM_SAFE
node_t*
TMdelete (rbtree_t* s, node_t* p);


#define RED   (0L)
#define BLACK (1L)


/*
 * See also:
 * - Doug Lea's j.u.TreeMap
 * - Keir Fraser's rb_stm.c and rb_lock_serialisedwriters.c in libLtx.
 *
 * Following Doug Lea's TreeMap example, we avoid the use of the magic
 * "nil" sentinel pointers.  The sentinel is simply a convenience and
 * is not fundamental to the algorithm.  We forgo the sentinel as
 * it is a source of false+ data conflicts in transactions.  Relatedly,
 * even with locks, use of a nil sentil can result in considerable
 * cache coherency traffic on traditional SMPs.
 */


/* =============================================================================
 * lookup
 * =============================================================================
 */
 TM_SAFE
node_t*
lookup (rbtree_t* s, void* k)
{
    node_t* p = LDNODE(s, root);

    while (p != NULL) {
        long cmp = s->compare(k, LDF(p, k));
        if (cmp == 0) {
            return p;
        }
        p = ((cmp < 0) ? LDNODE(p, l) : LDNODE(p, r));
    }

    return NULL;
}
#define LOOKUP(set, key)  lookup(set, key)


/* =============================================================================
 * TMlookup
 * =============================================================================
 */
 TM_SAFE
node_t*
TMlookup (  rbtree_t* s, void* k)
{
    void* k2;
    //[wer210] compare should be safe
    //long (*compare)(const void*, const void*) TM_IFUNC_DECL = s->compare;
    long (*compare)(const void*, const void*) TM_SAFE = s->compare;
    node_t* p = TX_LDNODE(s, root);

    while (p != NULL) {
        long cmp;
        k2 = TX_LDF_P(p, k);
        TM_IFUNC_CALL2(cmp, compare, k, k2);
        if (cmp == 0) {
            return p;
        }
        p = ((cmp < 0) ? TX_LDNODE(p, l) : TX_LDNODE(p, r));
    }

    return NULL;
}
#define TX_LOOKUP(set, key)  TMlookup(  set, key)


/*
 * Balancing operations.
 *
 * Implementations of rebalancings during insertion and deletion are
 * slightly different than the CLR version.  Rather than using dummy
 * nilnodes, we use a set of accessors that deal properly with null.  They
 * are used to avoid messiness surrounding nullness checks in the main
 * algorithms.
 *
 * From CLR
 */

/* =============================================================================
 * TMrotateLeft
 * =============================================================================
 */
 TM_SAFE
void
TMrotateLeft (  rbtree_t* s, node_t* x)
{
    node_t* r = TX_LDNODE(x, r); /* AKA r, y */
    node_t* rl = TX_LDNODE(r, l);
    TX_STF_P(x, r, rl);
    if (rl != NULL) {
        TX_STF_P(rl, p, x);
    }
    /* TODO: compute p = xp = x->p.  Use xp for R-Values in following */
    node_t* xp = TX_LDNODE(x, p);
    TX_STF_P(r, p, xp);
    if (xp == NULL) {
        TX_STF_P(s, root, r);
    } else if (TX_LDNODE(xp, l) == x) {
        TX_STF_P(xp, l, r);
    } else {
        TX_STF_P(xp, r, r);
    }
    TX_STF_P(r, l, x);
    TX_STF_P(x, p, r);
}
#define TX_ROTATE_LEFT(set, node)  TMrotateLeft(  set, node)


/* =============================================================================
 * TMrotateRight
 * =============================================================================
 */
 TM_SAFE
void
TMrotateRight (  rbtree_t* s, node_t* x)
{
    node_t* l = TX_LDNODE(x, l); /* AKA l,y */
    node_t* lr = TX_LDNODE(l, r);
    TX_STF_P(x, l, lr);
    if (lr != NULL) {
        TX_STF_P(lr, p, x);
    }
    node_t* xp = TX_LDNODE(x, p);
    TX_STF_P(l, p, xp);
    if (xp == NULL) {
        TX_STF_P(s, root, l);
    } else if (TX_LDNODE(xp, r) == x) {
        TX_STF_P(xp, r, l);
    } else {
        TX_STF_P(xp, l, l);
    }
    TX_STF_P(l, r, x);
    TX_STF_P(x, p, l);
}
#define TX_ROTATE_RIGHT(set, node)  TMrotateRight(  set, node)


/* =============================================================================
 * parentOf
 * =============================================================================
 */
static inline node_t*
parentOf (node_t* n)
{
   return (n ? LDNODE(n,p) : NULL);
}
#define PARENT_OF(n) parentOf(n)


/* =============================================================================
 * TMparentOf
 * =============================================================================
 */
 TM_SAFE
inline node_t*
TMparentOf (  node_t* n)
{
   return (n ? TX_LDNODE(n,p) : NULL);
}
#define TX_PARENT_OF(n)  TMparentOf(  n)


/* =============================================================================
 * leftOf
 * =============================================================================
 */
static inline node_t*
leftOf (node_t* n)
{
   return (n ? LDNODE(n, l) : NULL);
}
#define LEFT_OF(n)  leftOf(n)


/* =============================================================================
 * TMleftOf
 * =============================================================================
 */
 TM_SAFE
inline node_t*
TMleftOf (  node_t* n)
{
   return (n ? TX_LDNODE(n, l) : NULL);
}
#define TX_LEFT_OF(n)  TMleftOf(  n)


/* =============================================================================
 * rightOf
 * =============================================================================
 */
static inline node_t*
rightOf (node_t* n)
{
    return (n ? LDNODE(n, r) : NULL);
}
#define RIGHT_OF(n)  rightOf(n)


/* =============================================================================
 * TMrightOf
 * =============================================================================
 */
 TM_SAFE
inline node_t*
TMrightOf (  node_t* n)
{
    return (n ? TX_LDNODE(n, r) : NULL);
}
#define TX_RIGHT_OF(n)  TMrightOf(  n)


/* =============================================================================
 * colorOf
 * =============================================================================
 */
static inline long
colorOf (node_t* n)
{
    return (n ? (long)LDNODE(n, c) : BLACK);
}
#define COLOR_OF(n)  colorOf(n)


/* =============================================================================
 * TMcolorOf
 * =============================================================================
 */
 TM_SAFE
inline long
TMcolorOf (  node_t* n)
{
    return (n ? (long)TX_LDF(n, c) : BLACK);
}
#define TX_COLOR_OF(n)  TMcolorOf(  n)


/* =============================================================================
 * setColor
 * =============================================================================
 */
static inline void
setColor (node_t* n, long c)
{
    if (n != NULL) {
        STF(n, c, c);
    }
}
#define SET_COLOR(n, c)  setColor(n, c)


/* =============================================================================
 * TMsetColor
 * =============================================================================
 */
 TM_SAFE
inline void
TMsetColor (  node_t* n, long c)
{
    if (n != NULL) {
        TX_STF(n, c, c);
    }
}
#define TX_SET_COLOR(n, c)  TMsetColor(  n, c)


/* =============================================================================
 * TMfixAfterInsertion
 * =============================================================================
 */
 TM_SAFE
void
TMfixAfterInsertion (  rbtree_t* s, node_t* x)
{
    TX_STF(x, c, RED);
    while (x != NULL && x != TX_LDNODE(s, root)) {
        node_t* xp = TX_LDNODE(x, p);
        if (TX_LDF(xp, c) != RED) {
            break;
        }
        /* TODO: cache g = ppx = TX_PARENT_OF(TX_PARENT_OF(x)) */
        if (TX_PARENT_OF(x) == TX_LEFT_OF(TX_PARENT_OF(TX_PARENT_OF(x)))) {
            node_t*  y = TX_RIGHT_OF(TX_PARENT_OF(TX_PARENT_OF(x)));
            if (TX_COLOR_OF(y) == RED) {
                TX_SET_COLOR(TX_PARENT_OF(x), BLACK);
                TX_SET_COLOR(y, BLACK);
                TX_SET_COLOR(TX_PARENT_OF(TX_PARENT_OF(x)), RED);
                x = TX_PARENT_OF(TX_PARENT_OF(x));
            } else {
                if (x == TX_RIGHT_OF(TX_PARENT_OF(x))) {
                    x = TX_PARENT_OF(x);
                    TX_ROTATE_LEFT(s, x);
                }
                TX_SET_COLOR(TX_PARENT_OF(x), BLACK);
                TX_SET_COLOR(TX_PARENT_OF(TX_PARENT_OF(x)), RED);
                if (TX_PARENT_OF(TX_PARENT_OF(x)) != NULL) {
                    TX_ROTATE_RIGHT(s, TX_PARENT_OF(TX_PARENT_OF(x)));
                }
            }
        } else {
            node_t* y = TX_LEFT_OF(TX_PARENT_OF(TX_PARENT_OF(x)));
            if (TX_COLOR_OF(y) == RED) {
                TX_SET_COLOR(TX_PARENT_OF(x), BLACK);
                TX_SET_COLOR(y, BLACK);
                TX_SET_COLOR(TX_PARENT_OF(TX_PARENT_OF(x)), RED);
                x = TX_PARENT_OF(TX_PARENT_OF(x));
            } else {
                if (x == TX_LEFT_OF(TX_PARENT_OF(x))) {
                    x = TX_PARENT_OF(x);
                    TX_ROTATE_RIGHT(s, x);
                }
                TX_SET_COLOR(TX_PARENT_OF(x),  BLACK);
                TX_SET_COLOR(TX_PARENT_OF(TX_PARENT_OF(x)), RED);
                if (TX_PARENT_OF(TX_PARENT_OF(x)) != NULL) {
                    TX_ROTATE_LEFT(s, TX_PARENT_OF(TX_PARENT_OF(x)));
                }
            }
        }
    }
    node_t* ro = TX_LDNODE(s, root);
    if (TX_LDF(ro, c) != BLACK) {
        TX_STF(ro, c, BLACK);
    }
}
#define TX_FIX_AFTER_INSERTION(s, x)  TMfixAfterInsertion(  s, x)

#if 0
/* =============================================================================
 * insert
 * =============================================================================
 */
 TM_SAFE
node_t*
insert (rbtree_t* s, void* k, void* v, node_t* n)
{
    node_t* t  = LDNODE(s, root);
    if (t == NULL) {
        if (n == NULL) {
            return NULL;
        }
        /* Note: the following STs don't really need to be transactional */
        STF(n, l, NULL);
        STF(n, r, NULL);
        STF(n, p, NULL);
        STF(n, k, k);
        STF(n, v, v);
        STF(n, c, BLACK);
        STF(s, root, n);
        return NULL;
    }

    for (;;) {
        long cmp = s->compare(k, LDF(t, k));
        if (cmp == 0) {
            return t;
        } else if (cmp < 0) {
            node_t* tl = LDNODE(t, l);
            if (tl != NULL) {
                t = tl;
            } else {
                STF(n, l, NULL);
                STF(n, r, NULL);
                STF(n, k, k);
                STF(n, v, v);
                STF(n, p, t);
                STF(t, l, n);
                TX_FIX_AFTER_INSERTION(s, n);
                return NULL;
            }
        } else { /* cmp > 0 */
            node_t* tr = LDNODE(t, r);
            if (tr != NULL) {
                t = tr;
            } else {
                STF(n, l, NULL);
                STF(n, r, NULL);
                STF(n, k, k);
                STF(n, v, v);
                STF(n, p, t);
                STF(t, r, n);
                TX_FIX_AFTER_INSERTION(s, n);
                return NULL;
            }
        }
    }
}
#define INSERT(s, k, v, n)  insert(s, k, v, n)
#endif

/* =============================================================================
 * TMinsert
 * =============================================================================
 */

 TM_SAFE
node_t*
TMinsert (  rbtree_t* s, void* k, void* v, node_t* n)
{
    void* k2;
    //long (*compare)(const void*, const void*) TM_IFUNC_DECL = s->compare;
    long (*compare)(const void*, const void*) TM_SAFE = s->compare;
    node_t* t = TX_LDNODE(s, root);

    if (t == NULL) {
        if (n == NULL) {
            return NULL;
        }
        /* Note: the following STs don't really need to be transactional */
        TX_STF_P(n, l, (node_t*)NULL);
        TX_STF_P(n, r, (node_t*)NULL);
        TX_STF_P(n, p, (node_t*)NULL);
        TX_STF_P(n, k, k);
        TX_STF_P(n, v, v);
        TX_STF(n, c, BLACK);
        TX_STF_P(s, root, n);
        return NULL;
    }

    for (;;) {
        long cmp;
        k2 = TX_LDF_P(t, k);
        TM_IFUNC_CALL2(cmp, compare, k, k2);
        if (cmp == 0) {
            return t;
        } else if (cmp < 0) {
            node_t* tl = TX_LDNODE(t, l);
            if (tl != NULL) {
                t = tl;
            } else {
                TX_STF_P(n, l, (node_t*)NULL);
                TX_STF_P(n, r, (node_t*)NULL);
                TX_STF_P(n, k, k);
                TX_STF_P(n, v, v);
                TX_STF_P(n, p, t);
                TX_STF_P(t, l, n);
                TX_FIX_AFTER_INSERTION(s, n);
                return NULL;
            }
        } else { /* cmp > 0 */
            node_t* tr = TX_LDNODE(t, r);
            if (tr != NULL) {
                t = tr;
            } else {
                TX_STF_P(n, l, (node_t*)NULL);
                TX_STF_P(n, r, (node_t*)NULL);
                TX_STF_P(n, k, k);
                TX_STF_P(n, v, v);
                TX_STF_P(n, p, t);
                TX_STF_P(t, r, n);
                TX_FIX_AFTER_INSERTION(s, n);
                return NULL;
            }
        }
    }
}
#define TX_INSERT(s, k, v, n)  TMinsert(  s, k, v, n)

/*
 * Return the given node's successor node---the node which has the
 * next key in the the left to right ordering. If the node has
 * no successor, a null pointer is returned rather than a pointer to
 * the nil node
 */


/* =============================================================================
 * successor
 * =============================================================================
 */
static node_t*
successor (node_t* t)
{
    if (t == NULL) {
        return NULL;
    } else if (LDNODE(t, r) != NULL) {
        node_t* p = LDNODE(t, r);
        while (LDNODE(p, l) != NULL) {
            p = LDNODE(p, l);
        }
        return p;
    } else {
        node_t* p = LDNODE(t, p);
        node_t* ch = t;
        while (p != NULL && ch == LDNODE(p, r)) {
            ch = p;
            p = LDNODE(p, p);
        }
        return p;
    }
}
#define SUCCESSOR(n)  successor(n)


/* =============================================================================
 * TMsuccessor
 * =============================================================================
 */
 TM_SAFE
node_t*
TMsuccessor  (  node_t* t)
{
    if (t == NULL) {
        return NULL;
    } else if (TX_LDNODE(t, r) != NULL) {
        node_t* p = TX_LDNODE(t,r);
        while (TX_LDNODE(p, l) != NULL) {
            p = TX_LDNODE(p, l);
        }
        return p;
    } else {
        node_t* p = TX_LDNODE(t, p);
        node_t* ch = t;
        while (p != NULL && ch == TX_LDNODE(p, r)) {
            ch = p;
            p = TX_LDNODE(p, p);
        }
        return p;
    }
}
#define TX_SUCCESSOR(n)  TMsuccessor(  n)


/* =============================================================================
 * TMfixAfterDeletion
 * =============================================================================
 */
 TM_SAFE
void
TMfixAfterDeletion  (  rbtree_t* s, node_t* x)
{
    while (x != TX_LDNODE(s,root) && TX_COLOR_OF(x) == BLACK) {
        if (x == TX_LEFT_OF(TX_PARENT_OF(x))) {
            node_t* sib = TX_RIGHT_OF(TX_PARENT_OF(x));
            if (TX_COLOR_OF(sib) == RED) {
                TX_SET_COLOR(sib, BLACK);
                TX_SET_COLOR(TX_PARENT_OF(x), RED);
                TX_ROTATE_LEFT(s, TX_PARENT_OF(x));
                sib = TX_RIGHT_OF(TX_PARENT_OF(x));
            }
            if (TX_COLOR_OF(TX_LEFT_OF(sib)) == BLACK &&
                TX_COLOR_OF(TX_RIGHT_OF(sib)) == BLACK) {
                TX_SET_COLOR(sib, RED);
                x = TX_PARENT_OF(x);
            } else {
                if (TX_COLOR_OF(TX_RIGHT_OF(sib)) == BLACK) {
                    TX_SET_COLOR(TX_LEFT_OF(sib), BLACK);
                    TX_SET_COLOR(sib, RED);
                    TX_ROTATE_RIGHT(s, sib);
                    sib = TX_RIGHT_OF(TX_PARENT_OF(x));
                }
                TX_SET_COLOR(sib, TX_COLOR_OF(TX_PARENT_OF(x)));
                TX_SET_COLOR(TX_PARENT_OF(x), BLACK);
                TX_SET_COLOR(TX_RIGHT_OF(sib), BLACK);
                TX_ROTATE_LEFT(s, TX_PARENT_OF(x));
                /* TODO: consider break ... */
                x = TX_LDNODE(s,root);
            }
        } else { /* symmetric */
            node_t* sib = TX_LEFT_OF(TX_PARENT_OF(x));

            if (TX_COLOR_OF(sib) == RED) {
                TX_SET_COLOR(sib, BLACK);
                TX_SET_COLOR(TX_PARENT_OF(x), RED);
                TX_ROTATE_RIGHT(s, TX_PARENT_OF(x));
                sib = TX_LEFT_OF(TX_PARENT_OF(x));
            }
            if (TX_COLOR_OF(TX_RIGHT_OF(sib)) == BLACK &&
                TX_COLOR_OF(TX_LEFT_OF(sib)) == BLACK) {
                TX_SET_COLOR(sib,  RED);
                x = TX_PARENT_OF(x);
            } else {
                if (TX_COLOR_OF(TX_LEFT_OF(sib)) == BLACK) {
                    TX_SET_COLOR(TX_RIGHT_OF(sib), BLACK);
                    TX_SET_COLOR(sib, RED);
                    TX_ROTATE_LEFT(s, sib);
                    sib = TX_LEFT_OF(TX_PARENT_OF(x));
                }
                TX_SET_COLOR(sib, TX_COLOR_OF(TX_PARENT_OF(x)));
                TX_SET_COLOR(TX_PARENT_OF(x), BLACK);
                TX_SET_COLOR(TX_LEFT_OF(sib), BLACK);
                TX_ROTATE_RIGHT(s, TX_PARENT_OF(x));
                /* TODO: consider break ... */
                x = TX_LDNODE(s, root);
            }
        }
    }

    if (x != NULL && TX_LDF(x,c) != BLACK) {
       TX_STF(x, c, BLACK);
    }
}
#define TX_FIX_AFTER_DELETION(s, n)  TMfixAfterDeletion(  s, n )


/* =============================================================================
 * TMdelete
 * =============================================================================
 */
 TM_SAFE
node_t*
TMdelete (  rbtree_t* s, node_t* p)
{
    /*
     * If strictly internal, copy successor's element to p and then make p
     * point to successor
     */
    if (TX_LDNODE(p, l) != NULL && TX_LDNODE(p, r) != NULL) {
        node_t* s = TX_SUCCESSOR(p);
        TX_STF_P(p,k, TX_LDF_P(s, k));
        TX_STF_P(p,v, TX_LDF_P(s, v));
        p = s;
    } /* p has 2 children */

    /* Start fixup at replacement node, if it exists */
    node_t* replacement =
        ((TX_LDNODE(p, l) != NULL) ? TX_LDNODE(p, l) : TX_LDNODE(p, r));

    if (replacement != NULL) {
        /* Link replacement to parent */
        /* TODO: precompute pp = p->p and substitute below ... */
        TX_STF_P(replacement, p, TX_LDNODE(p, p));
        node_t* pp = TX_LDNODE(p, p);
        if (pp == NULL) {
            TX_STF_P(s, root, replacement);
        } else if (p == TX_LDNODE(pp, l)) {
            TX_STF_P(pp, l, replacement);
        } else {
            TX_STF_P(pp, r, replacement);
        }

        /* Null out links so they are OK to use by fixAfterDeletion */
        TX_STF_P(p, l, (node_t*)NULL);
        TX_STF_P(p, r, (node_t*)NULL);
        TX_STF_P(p, p, (node_t*)NULL);

        /* Fix replacement */
        if (TX_LDF(p,c) == BLACK) {
            TX_FIX_AFTER_DELETION(s, replacement);
        }
    } else if (TX_LDNODE(p,p) == NULL) { /* return if we are the only node */
        TX_STF_P(s, root, (node_t*)NULL);
    } else { /* No children. Use self as phantom replacement and unlink */
        if (TX_LDF(p,c) == BLACK) {
            TX_FIX_AFTER_DELETION(s, p);
        }
        node_t* pp = TX_LDNODE(p, p);
        if (pp != NULL) {
            if (p == TX_LDNODE(pp, l)) {
                TX_STF_P(pp,l, (node_t*)NULL);
            } else if (p == TX_LDNODE(pp, r)) {
                TX_STF_P(pp, r, (node_t*)NULL);
            }
            TX_STF_P(p, p, (node_t*)NULL);
        }
    }
    return p;
}
#define TX_DELETE(s, n)  TMdelete(  s, n)


/*
 * Diagnostic section
 */


/* =============================================================================
 * firstEntry
 * =============================================================================
 */
static node_t*
firstEntry (rbtree_t* s)
{
    node_t* p = s->root;
    if (p != NULL) {
        while (p->l != NULL) {
            p = p->l;
        }
    }
    return p;
}


#if 0
/* =============================================================================
 * predecessor
 * =============================================================================
 */
static node_t*
predecessor (node_t* t)
{
    if (t == NULL)
        return NULL;
    else if (t->l != NULL) {
        node_t* p = t->l;
        while (p->r != NULL) {
            p = p->r;
        }
        return p;
    } else {
        node_t* p = t->p;
        node_t* ch = t;
        while (p != NULL && ch == p->l) {
            ch = p;
            p = p->p;
        }
        return p;
    }
}
#endif


/*
 * Compute the BH (BlackHeight) and validate the tree.
 *
 * This function recursively verifies that the given binary subtree satisfies
 * three of the red black properties. It checks that every red node has only
 * black children. It makes sure that each node is either red or black. And it
 * checks that every path has the same count of black nodes from root to leaf.
 * It returns the blackheight of the given subtree; this allows blackheights to
 * be computed recursively and compared for left and right siblings for
 * mismatches. It does not check for every nil node being black, because there
 * is only one sentinel nil node. The return value of this function is the
 * black height of the subtree rooted at the node ``root'', or zero if the
 * subtree is not red-black.
 *
 */


/* =============================================================================
 * verifyRedBlack
 * [wer] Only used in the test case below.
 * =============================================================================
 */
static long
verifyRedBlack (node_t* root, long depth)
{
    long height_left;
    long height_right;

    if (root == NULL) {
        return 1;
    }

    height_left  = verifyRedBlack(root->l, depth+1);
    height_right = verifyRedBlack(root->r, depth+1);
    if (height_left == 0 || height_right == 0) {
        return 0;
    }
    if (height_left != height_right) {
        printf(" Imbalance @depth=%ld : %ld %ld\n", depth, height_left, height_right);
    }

    if (root->l != NULL && root->l->p != root) {
       printf(" lineage\n");
    }
    if (root->r != NULL && root->r->p != root) {
       printf(" lineage\n");
    }

    /* Red-Black alternation */
    if (root->c == RED) {
        if (root->l != NULL && root->l->c != BLACK) {
          printf("VERIFY %d\n", __LINE__);
          return 0;
        }
        if (root->r != NULL && root->r->c != BLACK) {
          printf("VERIFY %d\n", __LINE__);
          return 0;
        }
        return height_left;
    }
    if (root->c != BLACK) {
        printf("VERIFY %d\n", __LINE__);
        return 0;
    }

    return (height_left + 1);
}


/* =============================================================================
 * rbtree_verify
 * [wer] Only used in the test case below.
 * =============================================================================
 */
long
rbtree_verify (rbtree_t* s, long verbose)
{
    node_t* root = s->root;
    if (root == NULL) {
        return 1;
    }
    if (verbose) {
       printf("Integrity check: ");
    }

    if (root->p != NULL) {
        printf("  (WARNING) root %lX parent=%lX\n",
               (unsigned long)root, (unsigned long)root->p);
        return -1;
    }
    if (root->c != BLACK) {
        printf("  (WARNING) root %lX color=%lX\n",
               (unsigned long)root, (unsigned long)root->c);
    }

    /* Weak check of binary-tree property */
    long ctr = 0;
    node_t* its = firstEntry(s);
    while (its != NULL) {
        ctr++;
        node_t* child = its->l;
        if (child != NULL && child->p != its) {
            printf("Bad parent\n");
        }
        child = its->r;
        if (child != NULL && child->p != its) {
            printf("Bad parent\n");
        }
        node_t* nxt = successor(its);
        if (nxt == NULL) {
            break;
        }
        if (s->compare(its->k, nxt->k) >= 0) {
            printf("Key order %lX (%ld %ld) %lX (%ld %ld)\n",
                   (unsigned long)its, (long)its->k, (long)its->v,
                   (unsigned long)nxt, (long)nxt->k, (long)nxt->v);
            return -3;
        }
        its = nxt;
    }

    long vfy = verifyRedBlack(root, 0);
    if (verbose) {
        printf(" Nodes=%ld Depth=%ld\n", ctr, vfy);
    }

    return vfy;
}


/* =============================================================================
 * compareKeysDefault
 * =============================================================================
 */
 TM_SAFE long
compareKeysDefault (const void* a, const void* b)
{
    return ((long)a - (long)b);
}


/* =============================================================================
 * rbtree_alloc
 * =============================================================================
 */
TM_SAFE
rbtree_t*
rbtree_alloc (long (*compare)(const void*, const void*))
{
    rbtree_t* n = (rbtree_t* )malloc(sizeof(*n));
    if (n) {
        n->compare = (compare ? compare : &compareKeysDefault);
        n->root = NULL;
    }
    return n;
}

/* =============================================================================
 * releaseNode
 * =============================================================================
 */
 TM_SAFE
void
releaseNode (node_t* n)
{
  free(n);
}


/* =============================================================================
 * freeTreeNode
 * =============================================================================
 */
 TM_SAFE
void
freeTreeNode (node_t* n)
{
    if (n) {
        freeTreeNode(n->l);
        freeTreeNode(n->r);
        releaseNode(n);
    }
}


/* =============================================================================
 * rbtree_free
 * =============================================================================
 */
TM_SAFE
void
rbtree_free (rbtree_t* r)
{
    freeTreeNode(r->root);
    free(r);
}



/* =============================================================================
 * getNode
 * =============================================================================
 */
 TM_SAFE
node_t*
getNode ()
{
    node_t* n = (node_t*)malloc(sizeof(*n));
    return n;
}


/* =============================================================================
 * rbtree_insert
 * -- Returns TRUE on success
 * =============================================================================
 */
TM_SAFE
bool_t
rbtree_insert (rbtree_t* r, void* key, void* val)
{
    node_t* node = getNode();
    node_t* ex = TX_INSERT(r, key, val, node);
    if (ex != NULL) {
        releaseNode(node);
    }
    return ((ex == NULL) ? TRUE : FALSE);
}


/* =============================================================================
 * rbtree_delete
 * -- Returns TRUE if key exists
 * =============================================================================
 */
TM_SAFE
bool_t
rbtree_delete (rbtree_t* r, void* key)
{
    node_t* node = NULL;
    node = LOOKUP(r, key);
    if (node != NULL) {
      node = TX_DELETE(r, node);
    }
    if (node != NULL) {
        releaseNode(node);
    }
    return ((node != NULL) ? TRUE : FALSE);
}


/* =============================================================================
 * rbtree_update
 * -- Return FALSE if had to insert node first
 * =============================================================================
 */
TM_SAFE
bool_t
rbtree_update (rbtree_t* r, void* key, void* val)
{
    node_t* nn = getNode();
    node_t* ex = TX_INSERT(r, key, val, nn);
    if (ex != NULL) {
        STF(ex, v, val);
        releaseNode(nn);
        return TRUE;
    }
    return FALSE;
}


/* =============================================================================
 * rbtree_get
 * =============================================================================
 */
TM_SAFE
void*
rbtree_get (rbtree_t* r, void* key) {
    node_t* n = LOOKUP(r, key);
    if (n != NULL) {
        void* val = LDF(n, v);
        return val;
    }
    return NULL;
}

/* =============================================================================
 * TMrbtree_contains
 * =============================================================================
 */
TM_SAFE
long
rbtree_contains (  rbtree_t* r, void* key)
{
    node_t* n = LOOKUP(r, key);
    return (n != NULL);
}


/* /////////////////////////////////////////////////////////////////////////////
 * TEST_RBTREE
 * /////////////////////////////////////////////////////////////////////////////
 */
#ifdef TEST_RBTREE


#include <assert.h>
#include <stdio.h>


static long
compare (const void* a, const void* b)
{
    return (*((const long*)a) - *((const long*)b));
}


static void
insertInt (rbtree_t* rbtreePtr, long* data)
{
    printf("Inserting: %li\n", *data);
    rbtree_insert(rbtreePtr, (void*)data, (void*)data);
    assert(*(long*)rbtree_get(rbtreePtr, (void*)data) == *data);
    assert(rbtree_verify(rbtreePtr, 0) > 0);
}


static void
removeInt (rbtree_t* rbtreePtr, long* data)
{
    printf("Removing: %li\n", *data);
    rbtree_delete(rbtreePtr, (void*)data);
    assert(rbtree_get(rbtreePtr, (void*)data) == NULL);
    assert(rbtree_verify(rbtreePtr, 0) > 0);
}


int
main ()
{
    long data[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7};
    long numData = sizeof(data) / sizeof(data[0]);
    long i;

    puts("Starting...");

    rbtree_t* rbtreePtr = rbtree_alloc(&compare);
    assert(rbtreePtr);

    for (i = 0; i < numData; i++) {
        insertInt(rbtreePtr, &data[i]);
    }

    for (i = 0; i < numData; i++) {
        removeInt(rbtreePtr, &data[i]);
    }

    rbtree_free(rbtreePtr);

    puts("Done.");

    return 0;
}


#endif /* TEST_RBTREE */

#ifdef __cplusplus
}
#endif

/* =============================================================================
 *
 * End of rbtree.c
 *
 * =============================================================================
 */
