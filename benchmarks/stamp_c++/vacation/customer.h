/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * customer.h: Representation of customer
 */

#pragma once

#include "list.h"
#include "reservation.h"

struct customer_t {
    long id;
    list_t* reservationInfoListPtr;

    __attribute__((transaction_safe))
    customer_t(long id);

    __attribute__((transaction_safe))
    ~customer_t();
};

/*
 * customer_addReservationInfo
 * -- Returns TRUE if success, else FALSE
 */
__attribute__((transaction_safe))
bool customer_addReservationInfo(customer_t* customerPtr,
                                 reservation_type_t type, long id, long price);

/*
 * customer_removeReservationInfo
 * -- Returns TRUE if success, else FALSE
 */
__attribute__((transaction_safe))
bool customer_removeReservationInfo(customer_t* customerPtr,
                                    reservation_type_t type, long id);

/*
 * customer_getBill
 * -- Returns total cost of reservations
 */
__attribute__((transaction_safe))
long customer_getBill(customer_t* customerPtr);
