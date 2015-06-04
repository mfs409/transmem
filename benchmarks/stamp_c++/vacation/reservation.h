/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * reservation.h: Representation of car, flight, and hotel relations
 */

#pragma once

enum reservation_type_t {
    RESERVATION_CAR,
    RESERVATION_FLIGHT,
    RESERVATION_ROOM,
    NUM_RESERVATION_TYPE
};

struct reservation_info_t {
    reservation_type_t type;
    long id;
    long price; /* holds price at time reservation was made */

    __attribute__((transaction_safe))
    reservation_info_t(reservation_type_t type, long id, long price);

    // NB: no need to provide destructor... default will do
};

struct reservation_t {
    long id;
    long numUsed;
    long numFree;
    long numTotal;
    long price;

    __attribute__((transaction_safe))
    reservation_t(long id, long price, long numTotal, bool* success);

    // NB: no need to provide destructor... default will do
};

/*
 * reservation_info_compare
 * -- Returns -1 if A < B, 0 if A = B, 1 if A > B
 */
__attribute__((transaction_safe))
long reservation_info_compare(reservation_info_t* aPtr, reservation_info_t* bPtr);

/*
 * reservation_addToTotal
 * -- Adds if 'num' > 0, removes if 'num' < 0;
 * -- Returns TRUE on success, else FALSE
 */
__attribute__((transaction_safe))
bool reservation_addToTotal(reservation_t* reservationPtr, long num, bool* success);

/*
 * reservation_make
 * -- Returns TRUE on success, else FALSE
 */
__attribute__((transaction_safe))
bool reservation_make(reservation_t* reservationPtr);

/*
 * reservation_cancel
 * -- Returns TRUE on success, else FALSE
 */
__attribute__((transaction_safe))
bool reservation_cancel(reservation_t* reservationPtr);


/*
 * reservation_updatePrice
 * -- Failure if 'price' < 0
 * -- Returns TRUE on success, else FALSE
 */
__attribute__((transaction_safe))
bool reservation_updatePrice(reservation_t* reservationPtr, long newPrice);
