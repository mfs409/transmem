/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * manager.h: Travel reservation resource manager
 */

#pragma once

#include "map.h"

struct manager_t {
    MAP_T* carTablePtr;
    MAP_T* roomTablePtr;
    MAP_T* flightTablePtr;
    MAP_T* customerTablePtr;

    manager_t();
    ~manager_t();
};

/* =============================================================================
 * ADMINISTRATIVE INTERFACE
 * =============================================================================
 */

/* =============================================================================
 * manager_addCar
 * -- Add cars to a city
 * -- Adding to an existing car overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_addCar (manager_t* managerPtr, long carId, long numCar, long price);


/* =============================================================================
 * manager_deleteCar
 * -- Delete cars from a city
 * -- Decreases available car count (those not allocated to a customer)
 * -- Fails if would make available car count negative
 * -- If decresed to 0, deletes entire entry
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_deleteCar (  manager_t* managerPtr, long carId, long numCar);


/* =============================================================================
 * manager_addRoom
 * -- Add rooms to a city
 * -- Adding to an existing room overwrites the price if 'price' >= 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_addRoom (manager_t* managerPtr, long roomId, long numRoom, long price);


/* =============================================================================
 * manager_deleteRoom
 * -- Delete rooms from a city
 * -- Decreases available room count (those not allocated to a customer)
 * -- Fails if would make available room count negative
 * -- If decresed to 0, deletes entire entry
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_deleteRoom (  manager_t* managerPtr, long roomId, long numRoom);


/* =============================================================================
 * manager_addFlight
 * -- Add seats to a flight
 * -- Adding to an existing flight overwrites the price if 'price' >= 0
 * -- Returns TRUE on success, FALSE on failure
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_addFlight (
                   manager_t* managerPtr, long flightId, long numSeat, long price);

bool
manager_addFlight_seq (manager_t* managerPtr, long flightId, long numSeat, long price);


/* =============================================================================
 * manager_deleteFlight
 * -- Delete an entire flight
 * -- Fails if customer has reservation on this flight
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_deleteFlight (  manager_t* managerPtr, long flightId);


/* =============================================================================
 * manager_addCustomer
 * -- If customer already exists, returns success
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_addCustomer (  manager_t* managerPtr, long customerId);


/* =============================================================================
 * manager_deleteCustomer
 * -- Delete this customer and associated reservations
 * -- If customer does not exist, returns success
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_deleteCustomer (  manager_t* managerPtr, long customerId);


/* =============================================================================
 * QUERY INTERFACE
 * =============================================================================
 */


/* =============================================================================
 * manager_queryCar
 * -- Return the number of empty seats on a car
 * -- Returns -1 if the car does not exist
 * =============================================================================
 */
__attribute__((transaction_safe))
long
manager_queryCar (  manager_t* managerPtr, long carId);


/* =============================================================================
 * manager_queryCarPrice
 * -- Return the price of the car
 * -- Returns -1 if the car does not exist
 * =============================================================================
 */
__attribute__((transaction_safe))
long
manager_queryCarPrice (  manager_t* managerPtr, long carId);


/* =============================================================================
 * manager_queryRoom
 * -- Return the number of empty seats on a room
 * -- Returns -1 if the room does not exist
 * =============================================================================
 */
__attribute__((transaction_safe))
long
manager_queryRoom (  manager_t* managerPtr, long roomId);


/* =============================================================================
 * manager_queryRoomPrice
 * -- Return the price of the room
 * -- Returns -1 if the room does not exist
 * =============================================================================
 */
__attribute__((transaction_safe))
long
manager_queryRoomPrice (  manager_t* managerPtr, long roomId);


/* =============================================================================
 * manager_queryFlight
 * -- Return the number of empty seats on a flight
 * -- Returns -1 if the flight does not exist
 * =============================================================================
 */
__attribute__((transaction_safe))
long
manager_queryFlight (  manager_t* managerPtr, long flightId);


/* =============================================================================
 * manager_queryFlightPrice
 * -- Return the price of the flight
 * -- Returns -1 if the flight does not exist
 * =============================================================================
 */
__attribute__((transaction_safe))
long
manager_queryFlightPrice (  manager_t* managerPtr, long flightId);


/* =============================================================================
 * manager_queryCustomerBill
 * -- Return the total price of all reservations held for a customer
 * -- Returns -1 if the customer does not exist
 * =============================================================================
 */
__attribute__((transaction_safe))
long
manager_queryCustomerBill (  manager_t* managerPtr, long customerId);


/* =============================================================================
 * RESERVATION INTERFACE
 * =============================================================================
 */


/* =============================================================================
 * manager_reserveCar
 * -- Returns failure if the car or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_reserveCar (
                    manager_t* managerPtr, long customerId, long carId);


/* =============================================================================
 * manager_reserveRoom
 * -- Returns failure if the room or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_reserveRoom (
                     manager_t* managerPtr, long customerId, long roomId);


/* =============================================================================
 * manager_reserveFlight
 * -- Returns failure if the flight or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_reserveFlight (manager_t* managerPtr, long customerId, long flightId);


/* =============================================================================
 * manager_cancelCar
 * -- Returns failure if the car, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_cancelCar (manager_t* managerPtr, long customerId, long carId);


/* =============================================================================
 * manager_cancelRoom
 * -- Returns failure if the room, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_cancelRoom (manager_t* managerPtr, long customerId, long roomId);


/* =============================================================================
 * manager_cancelFlight
 * -- Returns failure if the flight, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe))
bool
manager_cancelFlight (manager_t* managerPtr, long customerId, long flightId);
