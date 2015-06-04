/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * customer.c: Representation of customer
 */

#include <assert.h>
#include <stdlib.h>
#include "customer.h"
#include "list.h"
#include "memory.h"
#include "reservation.h"
#include "tm_transition.h"

/* =============================================================================
 * compareReservationInfo
 * =============================================================================
 */
//static
__attribute__((transaction_safe)) long
compareReservationInfo (const void* aPtr, const void* bPtr)
{
    return reservation_info_compare((reservation_info_t*)aPtr,
                                    (reservation_info_t*)bPtr);
}


/* =============================================================================
 * customer_alloc
 * =============================================================================
 */
__attribute__((transaction_safe))
customer_t::customer_t(long _id)
{
    id = _id;

    // NB: must initialize with TM_SAFE compare function
    reservationInfoListPtr = TMLIST_ALLOC(&compareReservationInfo);
    assert(reservationInfoListPtr != NULL);
}


/* =============================================================================
 * customer_free
 * =============================================================================
 */
__attribute__((transaction_safe))
customer_t::~customer_t()
{
    // [mfs] Is this sufficient?  Does it free the whole list?
    TMLIST_FREE(reservationInfoListPtr);
}


/* =============================================================================
 * customer_addReservationInfo
 * -- Returns TRUE if success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
customer_addReservationInfo (customer_t* customerPtr,
                             reservation_type_t type, long id, long price)
{
    reservation_info_t* reservationInfoPtr =
        new reservation_info_t(type, id, price);

    list_t* reservationInfoListPtr =
        customerPtr->reservationInfoListPtr;

    return TMLIST_INSERT(reservationInfoListPtr, (void*)reservationInfoPtr);
}


/* =============================================================================
 * customer_removeReservationInfo
 * -- Returns TRUE if success, else FALSE
 * =============================================================================
 */
//[wer210] called only in manager.c, cancel() which is used to cancel a
//         flight/car/room, which never happens...
__attribute__((transaction_safe)) bool
customer_removeReservationInfo (customer_t* customerPtr,
                                reservation_type_t type, long id)
{
    // NB: price not used to compare reservation infos
    reservation_info_t findReservationInfo(type, id, 0);

    list_t* reservationInfoListPtr = customerPtr->reservationInfoListPtr;

    reservation_info_t* reservationInfoPtr =
        (reservation_info_t*)TMLIST_FIND(reservationInfoListPtr,
                                         &findReservationInfo);

    if (reservationInfoPtr == NULL) {
        return false;
    }
    bool status = TMLIST_REMOVE(reservationInfoListPtr,
                                  (void*)&findReservationInfo);

    //[wer210] get rid of restart()
    if (status == false) {
      //_ITM_abortTransaction(2);
      return false;
    }

    delete reservationInfoPtr;

    return true;
}


/* =============================================================================
 * customer_getBill
 * -- Returns total cost of reservations
 * =============================================================================
 */
__attribute__((transaction_safe)) long
customer_getBill (  customer_t* customerPtr)
{
    long bill = 0;
    list_iter_t it;
    list_t* reservationInfoListPtr = customerPtr->reservationInfoListPtr;

    TMLIST_ITER_RESET(&it, reservationInfoListPtr);
    while (TMLIST_ITER_HASNEXT(&it)) {
        reservation_info_t* reservationInfoPtr =
            (reservation_info_t*)TMLIST_ITER_NEXT(&it);
        bill += reservationInfoPtr->price;
    }

    return bill;
}


/* =============================================================================
 * TEST_CUSTOMER
 * =============================================================================
 */
#ifdef TEST_CUSTOMER


#include <assert.h>
#include <stdio.h>


int
main ()
{
    customer_t* customer1Ptr;
    customer_t* customer2Ptr;
    customer_t* customer3Ptr;

    assert(memory_init(1, 4, 2));

    puts("Starting...");

    customer1Ptr = customer_alloc(314);
    customer2Ptr = customer_alloc(314);
    customer3Ptr = customer_alloc(413);

    /* Test compare */
    /* =============================================================================
     * customer_compare
     * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
     * REMOVED, never used except here.
     * =============================================================================
     */
    //assert(customer_compare(customer1Ptr, customer2Ptr) == 0);
    assert((customer1Ptr->id - customer2Ptr->id) == 0);
    assert((customer2Ptr->id, customer3Ptr->id) != 0);
    assert((customer1Ptr->id, customer3Ptr->id) != 0);

    /* Test add reservation info */
    assert(customer_addReservationInfo(customer1Ptr, 0, 1, 2));
    assert(!customer_addReservationInfo(customer1Ptr, 0, 1, 2));
    assert(customer_addReservationInfo(customer1Ptr, 1, 1, 3));
    assert(customer_getBill(customer1Ptr) == 5);

    /* Test remove reservation info */
    assert(!customer_removeReservationInfo(customer1Ptr, 0, 2));
    assert(!customer_removeReservationInfo(customer1Ptr, 2, 0));
    assert(customer_removeReservationInfo(customer1Ptr, 0, 1));
    assert(!customer_removeReservationInfo(customer1Ptr, 0, 1));
    assert(customer_getBill(customer1Ptr) == 3);
    assert(customer_removeReservationInfo(customer1Ptr, 1, 1));
    assert(customer_getBill(customer1Ptr) == 0);

    customer_free(customer1Ptr);
    customer_free(customer2Ptr);
    customer_free(customer3Ptr);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_CUSTOMER */
