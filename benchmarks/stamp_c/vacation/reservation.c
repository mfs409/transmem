/* =============================================================================
 *
 * reservation.c
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


#include <assert.h>
#include <stdlib.h>
#include "memory.h"
#include "reservation.h"
#include "tm.h"
#include "types.h"

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */
//static void
TM_SAFE
bool_t
checkReservation (  reservation_t* reservationPtr);

/* =============================================================================
 * reservation_info_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
TM_SAFE reservation_info_t*
reservation_info_alloc (  reservation_type_t type, long id, long price)
{
    reservation_info_t* reservationInfoPtr;

    reservationInfoPtr = (reservation_info_t*)malloc(sizeof(reservation_info_t));
    if (reservationInfoPtr != NULL) {
        reservationInfoPtr->type = type;
        reservationInfoPtr->id = id;
        reservationInfoPtr->price = price;
    }

    return reservationInfoPtr;
}


/* =============================================================================
 * reservation_info_free
 * =============================================================================
 */
TM_SAFE void
reservation_info_free (  reservation_info_t* reservationInfoPtr)
{
    free(reservationInfoPtr);
}


/* =============================================================================
 * reservation_info_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 * =============================================================================
 */
TM_SAFE
long
reservation_info_compare (reservation_info_t* aPtr, reservation_info_t* bPtr)
{
    long typeDiff;

    typeDiff = aPtr->type - bPtr->type;

    return ((typeDiff != 0) ? (typeDiff) : (aPtr->id - bPtr->id));
}


//static void
TM_SAFE
bool_t
checkReservation (  reservation_t* reservationPtr)
{
    long numUsed = (long)TM_SHARED_READ(reservationPtr->numUsed);
    if (numUsed < 0) {
      //_ITM_abortTransaction(2);
      return FALSE;
    }

    long numFree = (long)TM_SHARED_READ(reservationPtr->numFree);
    if (numFree < 0) {
      //_ITM_abortTransaction(2);
      return FALSE;
    }

    long numTotal = (long)TM_SHARED_READ(reservationPtr->numTotal);
    if (numTotal < 0) {
      //_ITM_abortTransaction(2);
      return FALSE;
    }

    if ((numUsed + numFree) != numTotal) {
      //_ITM_abortTransaction(2);
      return FALSE;
    }

    long price = (long)TM_SHARED_READ(reservationPtr->price);
    if (price < 0) {
      //_ITM_abortTransaction(2);
      return FALSE;
    }

    return TRUE;
}
#define CHECK_RESERVATION(reservation) \
    checkReservation(reservation)

/* =============================================================================
 * reservation_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
TM_SAFE reservation_t*
reservation_alloc (long id, long numTotal, long price, bool_t* success)
{
    reservation_t* reservationPtr;

    reservationPtr = (reservation_t*)malloc(sizeof(reservation_t));
    if (reservationPtr != NULL) {
        reservationPtr->id = id;
        reservationPtr->numUsed = 0;
        reservationPtr->numFree = numTotal;
        reservationPtr->numTotal = numTotal;
        reservationPtr->price = price;
        *success = CHECK_RESERVATION(reservationPtr);
    }

    return reservationPtr;
}


/* =============================================================================
 * reservation_addToTotal
 * -- Adds if 'num' > 0, removes if 'num' < 0;
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
TM_SAFE bool_t
reservation_addToTotal (  reservation_t* reservationPtr, long num, bool_t* success)
{
  long numFree = (long)TM_SHARED_READ(reservationPtr->numFree);

    if (numFree + num < 0) {
        return FALSE;
    }

    TM_SHARED_WRITE(reservationPtr->numFree, (numFree + num));

    TM_SHARED_WRITE(reservationPtr->numTotal,
                    ((long)TM_SHARED_READ(reservationPtr->numTotal) + num));

    *success = CHECK_RESERVATION(reservationPtr);

    return TRUE;
}


/* =============================================================================
 * reservation_make
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
TM_SAFE bool_t
reservation_make (  reservation_t* reservationPtr)
{
    long numFree = (long)TM_SHARED_READ(reservationPtr->numFree);

    if (numFree < 1) {
        return FALSE;
    }

    TM_SHARED_WRITE(reservationPtr->numUsed,
                    ((long)TM_SHARED_READ(reservationPtr->numUsed) + 1));

    TM_SHARED_WRITE(reservationPtr->numFree, (numFree - 1));

    CHECK_RESERVATION(reservationPtr);
    return TRUE;
}


/* =============================================================================
 * reservation_cancel
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
TM_SAFE bool_t
reservation_cancel (reservation_t* reservationPtr)
{
    long numUsed = (long)TM_SHARED_READ(reservationPtr->numUsed);

    if (numUsed < 1) {
        return FALSE;
    }

    TM_SHARED_WRITE(reservationPtr->numUsed, (numUsed - 1));
    TM_SHARED_WRITE(reservationPtr->numFree,
    ((long)TM_SHARED_READ(reservationPtr->numFree) + 1));

    //[wer210] Note here, return false, instead of abort in check_reservation
    if (CHECK_RESERVATION(reservationPtr) == FALSE)
      return FALSE;

    return TRUE;
}



/* =============================================================================
 * reservation_updatePrice
 * -- Failure if 'price' < 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] returns were not used before, so use it to indicate aborts
TM_SAFE bool_t
reservation_updatePrice (  reservation_t* reservationPtr, long newPrice)
{
    if (newPrice < 0) {
      //return FALSE;
      return TRUE;
    }

    TM_SHARED_WRITE(reservationPtr->price, newPrice);

    //[wer210]
    if (CHECK_RESERVATION(reservationPtr))
      return TRUE;
    else return FALSE;
}



/* =============================================================================
 * reservation_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 * =============================================================================
 */
static
long
reservation_compare (reservation_t* aPtr, reservation_t* bPtr)
{
    return (aPtr->id - bPtr->id);
}



/* =============================================================================
 * reservation_free
 * =============================================================================
 */
TM_SAFE void
reservation_free (reservation_t* reservationPtr)
{
    free(reservationPtr);
}


/* =============================================================================
 * TEST_RESERVATION
 * =============================================================================
 */
#ifdef TEST_RESERVATION


#include <assert.h>
#include <stdio.h>


int
main ()
{
    reservation_info_t* reservationInfo1Ptr;
    reservation_info_t* reservationInfo2Ptr;
    reservation_info_t* reservationInfo3Ptr;

    reservation_t* reservation1Ptr;
    reservation_t* reservation2Ptr;
    reservation_t* reservation3Ptr;

    assert(memory_init(1, 4, 2));

    puts("Starting...");

    reservationInfo1Ptr = reservation_info_alloc(0, 0, 0);
    reservationInfo2Ptr = reservation_info_alloc(0, 0, 1);
    reservationInfo3Ptr = reservation_info_alloc(2, 0, 1);

    /* Test compare */
    assert(reservation_info_compare(reservationInfo1Ptr, reservationInfo2Ptr) == 0);
    assert(reservation_info_compare(reservationInfo1Ptr, reservationInfo3Ptr) > 0);
    assert(reservation_info_compare(reservationInfo2Ptr, reservationInfo3Ptr) > 0);

    reservation1Ptr = reservation_alloc(0, 0, 0);
    reservation2Ptr = reservation_alloc(0, 0, 1);
    reservation3Ptr = reservation_alloc(2, 0, 1);

    /* Test compare */
    assert(reservation_compare(reservation1Ptr, reservation2Ptr) == 0);
    assert(reservation_compare(reservation1Ptr, reservation3Ptr) != 0);
    assert(reservation_compare(reservation2Ptr, reservation3Ptr) != 0);

    /* Cannot reserve if total is 0 */
    assert(!reservation_make(reservation1Ptr));

    /* Cannot cancel if used is 0 */
    assert(!reservation_cancel(reservation1Ptr));

    /* Cannot update with negative price */
    assert(!reservation_updatePrice(reservation1Ptr, -1));

    /* Cannot make negative total */
    assert(!reservation_addToTotal(reservation1Ptr, -1));

    /* Update total and price */
    assert(reservation_addToTotal(reservation1Ptr, 1));
    assert(reservation_updatePrice(reservation1Ptr, 1));
    assert(reservation1Ptr->numUsed == 0);
    assert(reservation1Ptr->numFree == 1);
    assert(reservation1Ptr->numTotal == 1);
    assert(reservation1Ptr->price == 1);
    checkReservation(reservation1Ptr);

    /* Make and cancel reservation */
    assert(reservation_make(reservation1Ptr));
    assert(reservation_cancel(reservation1Ptr));
    assert(!reservation_cancel(reservation1Ptr));

    reservation_info_free(reservationInfo1Ptr);
    reservation_info_free(reservationInfo2Ptr);
    reservation_info_free(reservationInfo3Ptr);

    reservation_free(reservation1Ptr);
    reservation_free(reservation2Ptr);
    reservation_free(reservation3Ptr);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_RESERVATION */


/* =============================================================================
 *
 * End of reservation.c
 *
 * =============================================================================
 */
