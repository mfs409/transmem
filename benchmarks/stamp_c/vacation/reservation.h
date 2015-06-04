/* =============================================================================
 *
 * reservation.h
 * -- Representation of car, flight, and hotel relations
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


#ifndef RESERVATION_H
#define RESERVATION_H 1


#include "tm.h"
#include "types.h"

typedef enum reservation_type {
    RESERVATION_CAR,
    RESERVATION_FLIGHT,
    RESERVATION_ROOM,
    NUM_RESERVATION_TYPE
} reservation_type_t;

typedef struct reservation_info {
    reservation_type_t type;
    long id;
    long price; /* holds price at time reservation was made */
} reservation_info_t;

typedef struct reservation {
    long id;
    long numUsed;
    long numFree;
    long numTotal;
    long price;
} reservation_t;


/* =============================================================================
 * reservation_info_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
TM_SAFE
reservation_info_t*
reservation_info_alloc (reservation_type_t type, long id, long price);


/* =============================================================================
 * reservation_info_free
 * =============================================================================
 */
TM_SAFE
void
reservation_info_free (  reservation_info_t* reservationInfoPtr);


/* =============================================================================
 * reservation_info_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 * =============================================================================
 */
//[wer] safe comparator
TM_SAFE
long
reservation_info_compare (reservation_info_t* aPtr, reservation_info_t* bPtr);


/* =============================================================================
 * reservation_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
TM_SAFE
reservation_t*
reservation_alloc (  long id, long price, long numTotal, bool_t* success);


/* =============================================================================
 * reservation_addToTotal
 * -- Adds if 'num' > 0, removes if 'num' < 0;
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
TM_SAFE
bool_t
reservation_addToTotal (  reservation_t* reservationPtr, long num, bool_t* success);


/* =============================================================================
 * reservation_make
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
TM_SAFE
bool_t
reservation_make (  reservation_t* reservationPtr);


/* =============================================================================
 * reservation_cancel
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
TM_SAFE
bool_t
reservation_cancel (  reservation_t* reservationPtr);


/* =============================================================================
 * reservation_updatePrice
 * -- Failure if 'price' < 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
TM_SAFE
bool_t
reservation_updatePrice (  reservation_t* reservationPtr, long newPrice);



/* =============================================================================
 * reservation_free
 * =============================================================================
 */
TM_SAFE
void
reservation_free (reservation_t* reservationPtr);


#define RESERVATION_INFO_ALLOC(type, id, price)    \
    reservation_info_alloc(  type, id, price)
#define RESERVATION_INFO_FREE(r)    reservation_info_free(  r)
#define RESERVATION_ALLOC(id, price, tot, success)      \
    reservation_alloc(  id, price, tot, success)
#define RESERVATION_ADD_TO_TOTAL(r, num, success)       \
    reservation_addToTotal(  r, num, success)
#define RESERVATION_MAKE(r)      reservation_make(  r)
#define RESERVATION_CANCEL(r)    reservation_cancel(  r)
#define RESERVATION_UPDATE_PRICE(r, price) \
    reservation_updatePrice(  r, price)
#define RESERVATION_FREE(r)      reservation_free(r)


#endif /* RESERVATION_H */


/* =============================================================================
 *
 * End of reservation.h
 *
 * =============================================================================
 */
