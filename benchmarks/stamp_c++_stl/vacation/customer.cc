/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * customer.c: Representation of customer
 */

#include <cassert>
#include <cstdlib>
#include "customer.h"
#include "tm_transition.h"

/* =============================================================================
 * customer_alloc
 * =============================================================================
 */
__attribute__((transaction_safe))
customer_t::customer_t(long _id)
{
    id = _id;

    // NB: must initialize with TM_SAFE compare function
    reservationInfoList = new std::set<reservation_info_t*,
                                       bool(*)(reservation_info_t*, reservation_info_t*)
                                       >(reservation_info_compare);
    assert(reservationInfoList != NULL);
}


/* =============================================================================
 * customer_free
 * =============================================================================
 */
__attribute__((transaction_safe))
customer_t::~customer_t()
{
    // [mfs] Is this sufficient?  Does it free the whole list?
    delete(reservationInfoList);
}


/* =============================================================================
 * customer_addReservationInfo
 * -- Returns TRUE if success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool customer_t::addReservationInfo (reservation_type_t type, long id, long price)
{
    reservation_info_t* reservationInfoPtr =
        new reservation_info_t(type, id, price);
    return reservationInfoList->insert(reservationInfoPtr).second;
}


/* =============================================================================
 * customer_removeReservationInfo
 * -- Returns TRUE if success, else FALSE
 * =============================================================================
 */
//[wer210] called only in manager.c, cancel() which is used to cancel a
//         flight/car/room, which never happens...
__attribute__((transaction_safe))
bool customer_t::removeReservationInfo(reservation_type_t type, long id)
{
    // NB: price not used to compare reservation infos
    reservation_info_t findReservationInfo(type, id, 0);

    reservation_info_t* reservationInfoPtr =
        *reservationInfoList->find(&findReservationInfo);

    if (reservationInfoPtr == NULL) {
        return false;
    }

    int num = reservationInfoList->erase(&findReservationInfo);
    bool status = num == 1;

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
__attribute__((transaction_safe))
long customer_t::getBill()
{
    long bill = 0;
    for (auto i : *reservationInfoList)
        bill += i->price;
    return bill;
}


/* =============================================================================
 * TEST_CUSTOMER
 * =============================================================================
 */
#ifdef TEST_CUSTOMER


#include <cassert>
#include <cstdio>


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
