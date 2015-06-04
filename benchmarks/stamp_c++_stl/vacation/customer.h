/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * customer.h: Representation of customer
 */

#pragma once

#include <set>
#include "reservation.h"

struct customer_t {
    long id;
    std::set<reservation_info_t*, bool(*)(reservation_info_t*, reservation_info_t*)>* reservationInfoList;

    __attribute__((transaction_safe))
    customer_t(long id);

    __attribute__((transaction_safe))
    ~customer_t();

    /*
     * customer_addReservationInfo
     * -- Returns TRUE if success, else FALSE
     */
    __attribute__((transaction_safe))
    bool addReservationInfo(reservation_type_t type, long id, long price);

    /*
     * customer_removeReservationInfo
     * -- Returns TRUE if success, else FALSE
     */
    __attribute__((transaction_safe))
    bool removeReservationInfo(reservation_type_t type, long id);

    /*
     * customer_getBill
     * -- Returns total cost of reservations
     */
    __attribute__((transaction_safe))
    long getBill();
};
