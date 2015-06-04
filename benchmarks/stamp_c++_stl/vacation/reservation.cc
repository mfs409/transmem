/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * reservation.c: Representation of car, flight, and hotel relations
 */

#include "reservation.h"

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */

/* =============================================================================
 * reservation_info_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
__attribute__((transaction_safe))
reservation_info_t::reservation_info_t(reservation_type_t _type,
                                       long _id,
                                       long _price)
{
    type = _type;
    id = _id;
    price = _price;
}

/* =============================================================================
 * reservation_info_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
reservation_info_compare(reservation_info_t* left, reservation_info_t* right)
{
    if (left->type == right->type)
        return left->id < right->id;
    else
        return left->type < right->type;
}

//static void
__attribute__((transaction_safe))
bool
reservation_t::checkReservation()
{
    if (numUsed < 0) {
      //_ITM_abortTransaction(2);
      return false;
    }

    if (numFree < 0) {
      //_ITM_abortTransaction(2);
      return false;
    }

    if (numTotal < 0) {
      //_ITM_abortTransaction(2);
      return false;
    }

    if ((numUsed + numFree) != numTotal) {
      //_ITM_abortTransaction(2);
      return false;
    }

    if (price < 0) {
      //_ITM_abortTransaction(2);
      return false;
    }

    return true;
}

/* =============================================================================
 * reservation_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
__attribute__((transaction_safe))
reservation_t::reservation_t(long _id,
                             long _numTotal,
                             long _price,
                             bool* success)
{
    id = _id;
    numUsed = 0;
    numFree = _numTotal;
    numTotal = _numTotal;
    price = _price;
    *success = checkReservation();
}


/* =============================================================================
 * reservation_t::addToTotal
 * -- Adds if 'num' > 0, removes if 'num' < 0;
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
reservation_t::addToTotal(long num, bool* success)
{
    if (numFree + num < 0)
        return false;

    numFree += num;
    numTotal += num;

    *success = checkReservation();
    return true;
}


/* =============================================================================
 * reservation_t::make
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool reservation_t::make()
{
    if (numFree < 1)
        return false;

    numUsed += 1;
    numFree -= 1;

    checkReservation();
    return true;
}


/* =============================================================================
 * reservation_t::cancel
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
reservation_t::cancel()
{
    if (numUsed < 1)
        return false;

    numUsed -= 1;
    numFree += 1;

    //[wer210] Note here, return false, instead of abort in checkReservation
    return checkReservation();
}



/* =============================================================================
 * reservation_t::updatePrice
 * -- Failure if 'price' < 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] returns were not used before, so use it to indicate aborts
__attribute__((transaction_safe)) bool
reservation_t::updatePrice(long newPrice)
{
    if (newPrice < 0) {
      //return FALSE;
      return true;
    }

    price = newPrice;

    return checkReservation();
}

/* =============================================================================
 * TEST_RESERVATION
 * =============================================================================
 */
#ifdef TEST_RESERVATION


#include <cassert>
#include <cstdio>

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
