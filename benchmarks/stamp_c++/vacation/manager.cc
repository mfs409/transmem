/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * manager.c: Travel reservation resource manager
 */

#include <assert.h>
#include <stdlib.h>
#include "customer.h"
#include "map.h"
#include "pair.h"
#include "manager.h"
#include "reservation.h"
#include "tm_transition.h"

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */
__attribute__((transaction_safe))
long
queryNumFree (  MAP_T* tablePtr, long id);

__attribute__((transaction_safe))
long
queryPrice (  MAP_T* tablePtr, long id);

__attribute__((transaction_safe))
bool
reserve ( MAP_T* tablePtr, MAP_T* customerTablePtr, long customerId, long id, reservation_type_t type);

__attribute__((transaction_safe))
bool
cancel ( MAP_T* tablePtr, MAP_T* customerTablePtr, long customerId, long id, reservation_type_t type);

__attribute__((transaction_safe))
bool
addReservation (  MAP_T* tablePtr, long id, long num, long price);

/**
 * Constructor for manager objects
 */
manager_t::manager_t()
{
    carTablePtr = MAP_ALLOC(NULL, NULL);
    roomTablePtr = MAP_ALLOC(NULL, NULL);
    flightTablePtr = MAP_ALLOC(NULL, NULL);
    customerTablePtr = MAP_ALLOC(NULL, NULL);
    // [mfs] Once map is a c++ object, these asserts are unnecessary
    assert(carTablePtr != NULL);
    assert(roomTablePtr != NULL);
    assert(flightTablePtr != NULL);
    assert(customerTablePtr != NULL);
}

/**
 * Destructor
 *
 * [mfs] notes in the earlier code suggest that contents are not deleted
 *       here.  That's bad, but we can't fix it yet.
 */
manager_t::~manager_t()
{
    MAP_FREE(carTablePtr);
    MAP_FREE(roomTablePtr);
    MAP_FREE(flightTablePtr);
    MAP_FREE(customerTablePtr);
}


/* =============================================================================
 * ADMINISTRATIVE INTERFACE
 * =============================================================================
 */


/* =============================================================================
 * addReservation
 * -- If 'num' > 0 then add, if < 0 remove
 * -- Adding 0 seats is error if does not exist
 * -- If 'price' < 0, do not update price
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] return value not used before, now indicationg aborts.
__attribute__((transaction_safe))
bool
addReservation (MAP_T* tablePtr, long id, long num, long price)
{
    reservation_t* reservationPtr;
    reservationPtr = (reservation_t*)TMMAP_FIND(tablePtr, id);

    bool success = true;
    if (reservationPtr == NULL) {
        /* Create new reservation */
        if (num < 1 || price < 0) {
          //return FALSE;
          return true;
        }

        //[wer210] there was aborts inside RESERVATION_ALLOC, passing an extra parameter.
        reservationPtr = new reservation_t(id, num, price, &success);
        if (!success) return false;

        assert(reservationPtr != NULL);
        TMMAP_INSERT(tablePtr, id, reservationPtr);
    } else {
      /* Update existing reservation */
      //[wer210] there was aborts inside RESERVATION_ADD_TO_TOTAL, passing an extra parameter.
      if (!reservation_addToTotal(reservationPtr, num, &success)) {
        //return FALSE;
        if (success)
          return true;
        else return false;
      }

      if (reservationPtr->numTotal == 0) {
        bool status = TMMAP_REMOVE(tablePtr, id);
        if (status == false) {
          //_ITM_abortTransaction(2);
          return false;
        }

        delete reservationPtr;
      } else {
        //[wer210] there was aborts inside RESERVATIOn_UPDATE_PRICE, and return was not used
        if (!reservation_updatePrice(reservationPtr, price))
          return false;
      }
    }

    return true;
}


/* =============================================================================
 * manager_addCar
 * -- Add cars to a city
 * -- Adding to an existing car overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Return value not used before.
__attribute__((transaction_safe)) bool
manager_addCar (manager_t* managerPtr, long carId, long numCars, long price)
{
    return addReservation(  managerPtr->carTablePtr, carId, numCars, price);
}


/* =============================================================================
 * manager_deleteCar
 * -- Delete cars from a city
 * -- Decreases available car count (those not allocated to a customer)
 * -- Fails if would make available car count negative
 * -- If decresed to 0, deletes entire entry
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_deleteCar (  manager_t* managerPtr, long carId, long numCar)
{
    /* -1 keeps old price */
    return addReservation(  managerPtr->carTablePtr, carId, -numCar, -1);
}


/* =============================================================================
 * manager_addRoom
 * -- Add rooms to a city
 * -- Adding to an existing room overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_addRoom (manager_t* managerPtr, long roomId, long numRoom, long price)
{
    return addReservation(  managerPtr->roomTablePtr, roomId, numRoom, price);
}



/* =============================================================================
 * manager_deleteRoom
 * -- Delete rooms from a city
 * -- Decreases available room count (those not allocated to a customer)
 * -- Fails if would make available room count negative
 * -- If decresed to 0, deletes entire entry
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_deleteRoom (manager_t* managerPtr, long roomId, long numRoom)
{
    /* -1 keeps old price */
    return addReservation(  managerPtr->roomTablePtr, roomId, -numRoom, -1);
}


/* =============================================================================
 * manager_addFlight
 * -- Add seats to a flight
 * -- Adding to an existing flight overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, FALSE on failure
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_addFlight (manager_t* managerPtr, long flightId, long numSeat, long price)
{
    return addReservation(managerPtr->flightTablePtr, flightId, numSeat, price);
}



/* =============================================================================
 * manager_deleteFlight
 * -- Delete an entire flight
 * -- Fails if customer has reservation on this flight
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] return not used before, make it to indicate aborts
__attribute__((transaction_safe)) bool
manager_deleteFlight (  manager_t* managerPtr, long flightId)
{
    reservation_t* reservationPtr;

    reservationPtr = (reservation_t*)TMMAP_FIND(managerPtr->flightTablePtr, flightId);
    if (reservationPtr == NULL) {
      //return FALSE;
      return true;
    }

    if (reservationPtr->numUsed > 0) {
      //return FALSE; /* somebody has a reservation */
      return true;
    }

    return addReservation(managerPtr->flightTablePtr,
                          flightId,
                          -1*reservationPtr->numTotal,
                          -1 /* -1 keeps old price */);
}


/* =============================================================================
 * manager_addCustomer
 * -- If customer already exists, returns failure
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Function is called inside a transaction in client.c
//         But return value of this function was never used,
//         so I make it to return true all the time except when need to abort.
__attribute__((transaction_safe)) bool
manager_addCustomer (  manager_t* managerPtr, long customerId)
{
    customer_t* customerPtr;
    bool status;

    if (TMMAP_CONTAINS(managerPtr->customerTablePtr, customerId)) {
      //return FALSE;
      return true;
    }

    customerPtr = new customer_t(customerId);
    assert(customerPtr != NULL);

    status = TMMAP_INSERT(managerPtr->customerTablePtr, customerId, customerPtr);
    if (status == false) {
      //_ITM_abortTransaction(2);
      return false;
    }

    return true;
}



/* =============================================================================
 * manager_deleteCustomer
 * -- Delete this customer and associated reservations
 * -- If customer does not exist, returns success
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Again, the return values were not used (except for test cases below)
//         So I make it alway returning true, unless need to abort a transaction.
__attribute__((transaction_safe)) bool
manager_deleteCustomer (  manager_t* managerPtr, long customerId)
{
    customer_t* customerPtr;
    MAP_T* reservationTables[NUM_RESERVATION_TYPE];
    list_t* reservationInfoListPtr;
    list_iter_t it;
    bool status;

    customerPtr = (customer_t*)TMMAP_FIND(managerPtr->customerTablePtr, customerId);
    if (customerPtr == NULL) {
      //return FALSE;
      return true;
    }

    reservationTables[RESERVATION_CAR] = managerPtr->carTablePtr;
    reservationTables[RESERVATION_ROOM] = managerPtr->roomTablePtr;
    reservationTables[RESERVATION_FLIGHT] = managerPtr->flightTablePtr;

    /* Cancel this customer's reservations */
    reservationInfoListPtr = customerPtr->reservationInfoListPtr;
    TMLIST_ITER_RESET(&it, reservationInfoListPtr);
    while (TMLIST_ITER_HASNEXT(&it)) {
      reservation_info_t* reservationInfoPtr =
        (reservation_info_t*)TMLIST_ITER_NEXT(&it);
      reservation_t* reservationPtr =
        (reservation_t*)TMMAP_FIND(reservationTables[reservationInfoPtr->type],
                                   reservationInfoPtr->id);
      if (reservationPtr == NULL) {
        //_ITM_abortTransaction(2);
        return false;
      }

      status = reservation_cancel(reservationPtr);
      if (status == false) {
        //_ITM_abortTransaction(2);
        return false;
      }
      delete reservationInfoPtr;
    }

    status = TMMAP_REMOVE(managerPtr->customerTablePtr, customerId);
    if (status == false) {
      //_ITM_abortTransaction(2);
      return false;
    }
    delete customerPtr;

    return true;
}


/* =============================================================================
 * QUERY INTERFACE
 * =============================================================================
 */


/* =============================================================================
 * queryNumFree
 * -- Return numFree of a reservation, -1 if failure
 * =============================================================================
 */
__attribute__((transaction_safe))
long
queryNumFree (  MAP_T* tablePtr, long id)
{
    long numFree = -1;
    reservation_t* reservationPtr;

    reservationPtr = (reservation_t*)TMMAP_FIND(tablePtr, id);
    if (reservationPtr != NULL) {
        numFree = reservationPtr->numFree;
    }

    return numFree;
}


/* =============================================================================
 * queryPrice
 * -- Return price of a reservation, -1 if failure
 * =============================================================================
 */
 __attribute__((transaction_safe))
long
queryPrice (  MAP_T* tablePtr, long id)
{
    long price = -1;
    reservation_t* reservationPtr = (reservation_t*)TMMAP_FIND(tablePtr, id);
    if (reservationPtr != NULL) {
        price = reservationPtr->price;
    }

    return price;
}


/* =============================================================================
 * manager_queryCar
 * -- Return the number of empty seats on a car
 * -- Returns -1 if the car does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_queryCar (manager_t* managerPtr, long carId)
{
    return queryNumFree(managerPtr->carTablePtr, carId);
}


/* =============================================================================
 * manager_queryCarPrice
 * -- Return the price of the car
 * -- Returns -1 if the car does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_queryCarPrice (manager_t* managerPtr, long carId)
{
    return queryPrice(managerPtr->carTablePtr, carId);
}


/* =============================================================================
 * manager_queryRoom
 * -- Return the number of empty seats on a room
 * -- Returns -1 if the room does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_queryRoom (  manager_t* managerPtr, long roomId)
{
    return queryNumFree(  managerPtr->roomTablePtr, roomId);
}


/* =============================================================================
 * manager_queryRoomPrice
 * -- Return the price of the room
 * -- Returns -1 if the room does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_queryRoomPrice (  manager_t* managerPtr, long roomId)
{
    return queryPrice(  managerPtr->roomTablePtr, roomId);
}


/* =============================================================================
 * manager_queryFlight
 * -- Return the number of empty seats on a flight
 * -- Returns -1 if the flight does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_queryFlight (  manager_t* managerPtr, long flightId)
{
    return queryNumFree(  managerPtr->flightTablePtr, flightId);
}


/* =============================================================================
 * manager_queryFlightPrice
 * -- Return the price of the flight
 * -- Returns -1 if the flight does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_queryFlightPrice (  manager_t* managerPtr, long flightId)
{
    return queryPrice(  managerPtr->flightTablePtr, flightId);
}


/* =============================================================================
 * manager_queryCustomerBill
 * -- Return the total price of all reservations held for a customer
 * -- Returns -1 if the customer does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_queryCustomerBill (  manager_t* managerPtr, long customerId)
{
    long bill = -1;
    customer_t* customerPtr;

    customerPtr = (customer_t*)TMMAP_FIND(managerPtr->customerTablePtr, customerId);

    if (customerPtr != NULL) {
        bill = customer_getBill(customerPtr);
    }

    return bill;
}


/* =============================================================================
 * RESERVATION INTERFACE
 * =============================================================================
 */


/* =============================================================================
 * reserve
 * -- Customer is not allowed to reserve same (type, id) multiple times
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Again, the original return values are not used. So I modified return values
// to indicate if should restart a transaction.
__attribute__((transaction_safe))
bool
reserve (MAP_T* tablePtr, MAP_T* customerTablePtr,
         long customerId, long id, reservation_type_t type)
{
    customer_t* customerPtr;
    reservation_t* reservationPtr;

    customerPtr = (customer_t*)TMMAP_FIND(customerTablePtr, customerId);
    if (customerPtr == NULL) {
      //return FALSE;
      return true;
    }

    reservationPtr = (reservation_t*)TMMAP_FIND(tablePtr, id);
    if (reservationPtr == NULL) {
      //return FALSE;
      return true;
    }

    if (!reservation_make(reservationPtr)) {
      //return FALSE;
      return true;
    }

    if (!customer_addReservationInfo(customerPtr, type, id,
                                     (long)reservationPtr->price))
    {
      /* Undo previous successful reservation */
      bool status = reservation_cancel(reservationPtr);
      if (status == false) {
        //_ITM_abortTransaction(2);
        return false;
      }
      //return FALSE;
    }
    return true;
}


/* =============================================================================
 * manager_reserveCar
 * -- Returns failure if the car or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_reserveCar (  manager_t* managerPtr, long customerId, long carId)
{
    return reserve(managerPtr->carTablePtr,
                   managerPtr->customerTablePtr,
                   customerId,
                   carId,
                   RESERVATION_CAR);
}


/* =============================================================================
 * manager_reserveRoom
 * -- Returns failure if the room or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_reserveRoom (  manager_t* managerPtr, long customerId, long roomId)
{
    return reserve(managerPtr->roomTablePtr,
                   managerPtr->customerTablePtr,
                   customerId,
                   roomId,
                   RESERVATION_ROOM);
}


/* =============================================================================
 * manager_reserveFlight
 * -- Returns failure if the flight or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_reserveFlight (manager_t* managerPtr, long customerId, long flightId)
{
    return reserve(managerPtr->flightTablePtr,
                   managerPtr->customerTablePtr,
                   customerId,
                   flightId,
                   RESERVATION_FLIGHT);
}


/* =============================================================================
 * cancel
 * -- Customer is not allowed to cancel multiple times
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] was a "static" function, invoked by three functions below
//         however, never called.
__attribute__((transaction_safe))
bool
cancel (MAP_T* tablePtr, MAP_T* customerTablePtr,
        long customerId, long id, reservation_type_t type)
{
    customer_t* customerPtr;
    reservation_t* reservationPtr;

    customerPtr = (customer_t*)TMMAP_FIND(customerTablePtr, customerId);
    if (customerPtr == NULL) {
        return false;
    }

    reservationPtr = (reservation_t*)TMMAP_FIND(tablePtr, id);
    if (reservationPtr == NULL) {
        return false;
    }

    if (!reservation_cancel(reservationPtr)) {
        return false;
    }

    if (!customer_removeReservationInfo(customerPtr, type, id)) {
        /* Undo previous successful cancellation */
      bool status = reservation_make(reservationPtr);
      if (status == false) {
        //_ITM_abortTransaction(2);
        return false;
      }
      return false;
    }
    return true;
}


/* =============================================================================
 * manager_cancelCar
 * -- Returns failure if the car, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_cancelCar (  manager_t* managerPtr, long customerId, long carId)
{
    return cancel(managerPtr->carTablePtr,
                  managerPtr->customerTablePtr,
                  customerId,
                  carId,
                  RESERVATION_CAR);
}


/* =============================================================================
 * manager_cancelRoom
 * -- Returns failure if the room, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_cancelRoom (  manager_t* managerPtr, long customerId, long roomId)
{
    return cancel(managerPtr->roomTablePtr,
                  managerPtr->customerTablePtr,
                  customerId,
                  roomId,
                  RESERVATION_ROOM);
}



/* =============================================================================
 * manager_cancelFlight
 * -- Returns failure if the flight, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_cancelFlight (manager_t* managerPtr, long customerId, long flightId)
{
    return cancel(managerPtr->flightTablePtr,
                  managerPtr->customerTablePtr,
                  customerId,
                  flightId,
                  RESERVATION_FLIGHT);
}


/* =============================================================================
 * TEST_MANAGER
 * =============================================================================
 */
#ifdef TEST_MANAGER


#include <assert.h>
#include <stdio.h>


int
main ()
{
    manager_t* managerPtr;

    assert(memory_init(1, 4, 2));

    puts("Starting...");

    managerPtr = manager_alloc();

    /* Test administrative interface for cars */
    assert(!manager_addCar(managerPtr, 0, -1, 0)); /* negative num */
    assert(!manager_addCar(managerPtr, 0, 0, -1)); /* negative price */
    assert(!manager_addCar(managerPtr, 0, 0, 0)); /* zero num */
    assert(manager_addCar(managerPtr, 0, 1, 1));
    assert(!manager_deleteCar(managerPtr, 1, 0)); /* does not exist */
    assert(!manager_deleteCar(managerPtr, 0, 2)); /* cannot remove that many */
    assert(manager_addCar(managerPtr, 0, 1, 0));
    assert(manager_deleteCar(managerPtr, 0, 1));
    assert(manager_deleteCar(managerPtr, 0, 1));
    assert(!manager_deleteCar(managerPtr, 0, 1)); /* none left */
    assert(manager_queryCar(managerPtr, 0) == -1); /* does not exist */

    /* Test administrative interface for rooms */
    assert(!manager_addRoom(managerPtr, 0, -1, 0)); /* negative num */
    assert(!manager_addRoom(managerPtr, 0, 0, -1)); /* negative price */
    assert(!manager_addRoom(managerPtr, 0, 0, 0)); /* zero num */
    assert(manager_addRoom(managerPtr, 0, 1, 1));
    assert(!manager_deleteRoom(managerPtr, 1, 0)); /* does not exist */
    assert(!manager_deleteRoom(managerPtr, 0, 2)); /* cannot remove that many */
    assert(manager_addRoom(managerPtr, 0, 1, 0));
    assert(manager_deleteRoom(managerPtr, 0, 1));
    assert(manager_deleteRoom(managerPtr, 0, 1));
    assert(!manager_deleteRoom(managerPtr, 0, 1)); /* none left */
    assert(manager_queryRoom(managerPtr, 0) ==  -1); /* does not exist */

    /* Test administrative interface for flights and customers */
    assert(!manager_addFlight(managerPtr, 0, -1, 0));  /* negative num */
    assert(!manager_addFlight(managerPtr, 0, 0, -1));  /* negative price */
    assert(!manager_addFlight(managerPtr, 0, 0, 0));
    assert(manager_addFlight(managerPtr, 0, 1, 0));
    assert(!manager_deleteFlight(managerPtr, 1)); /* does not exist */
    assert(!manager_deleteFlight(managerPtr, 2)); /* cannot remove that many */
    assert(!manager_cancelFlight(managerPtr, 0, 0)); /* do not have reservation */
    assert(!manager_reserveFlight(managerPtr, 0, 0)); /* customer does not exist */
    assert(!manager_deleteCustomer(managerPtr, 0)); /* does not exist */
    assert(manager_addCustomer(managerPtr, 0));
    assert(!manager_addCustomer(managerPtr, 0)); /* already exists */
    assert(manager_reserveFlight(managerPtr, 0, 0));
    assert(manager_addFlight(managerPtr, 0, 1, 0));
    assert(!manager_deleteFlight(managerPtr, 0)); /* someone has reservation */
    assert(manager_cancelFlight(managerPtr, 0, 0));
    assert(manager_deleteFlight(managerPtr, 0));
    assert(!manager_deleteFlight(managerPtr, 0)); /* does not exist */
    assert(manager_queryFlight(managerPtr, 0) ==  -1); /* does not exist */
    assert(manager_deleteCustomer(managerPtr, 0));

    /* Test query interface for cars */
    assert(manager_addCustomer(managerPtr, 0));
    assert(manager_queryCar(managerPtr, 0) == -1); /* does not exist */
    assert(manager_queryCarPrice(managerPtr, 0) == -1); /* does not exist */
    assert(manager_addCar(managerPtr, 0, 1, 2));
    assert(manager_queryCar(managerPtr, 0) == 1);
    assert(manager_queryCarPrice(managerPtr, 0) == 2);
    assert(manager_addCar(managerPtr, 0, 1, -1));
    assert(manager_queryCar(managerPtr, 0) == 2);
    assert(manager_reserveCar(managerPtr, 0, 0));
    assert(manager_queryCar(managerPtr, 0) == 1);
    assert(manager_deleteCar(managerPtr, 0, 1));
    assert(manager_queryCar(managerPtr, 0) == 0);
    assert(manager_queryCarPrice(managerPtr, 0) == 2);
    assert(manager_addCar(managerPtr, 0, 1, 1));
    assert(manager_queryCarPrice(managerPtr, 0) == 1);
    assert(manager_deleteCustomer(managerPtr, 0));
    assert(manager_queryCar(managerPtr, 0) == 2);
    assert(manager_deleteCar(managerPtr, 0, 2));

    /* Test query interface for rooms */
    assert(manager_addCustomer(managerPtr, 0));
    assert(manager_queryRoom(managerPtr, 0) == -1); /* does not exist */
    assert(manager_queryRoomPrice(managerPtr, 0) == -1); /* does not exist */
    assert(manager_addRoom(managerPtr, 0, 1, 2));
    assert(manager_queryRoom(managerPtr, 0) == 1);
    assert(manager_queryRoomPrice(managerPtr, 0) == 2);
    assert(manager_addRoom(managerPtr, 0, 1, -1));
    assert(manager_queryRoom(managerPtr, 0) == 2);
    assert(manager_reserveRoom(managerPtr, 0, 0));
    assert(manager_queryRoom(managerPtr, 0) == 1);
    assert(manager_deleteRoom(managerPtr, 0, 1));
    assert(manager_queryRoom(managerPtr, 0) == 0);
    assert(manager_queryRoomPrice(managerPtr, 0) == 2);
    assert(manager_addRoom(managerPtr, 0, 1, 1));
    assert(manager_queryRoomPrice(managerPtr, 0) == 1);
    assert(manager_deleteCustomer(managerPtr, 0));
    assert(manager_queryRoom(managerPtr, 0) == 2);
    assert(manager_deleteRoom(managerPtr, 0, 2));

    /* Test query interface for flights */
    assert(manager_addCustomer(managerPtr, 0));
    assert(manager_queryFlight(managerPtr, 0) == -1); /* does not exist */
    assert(manager_queryFlightPrice(managerPtr, 0) == -1); /* does not exist */
    assert(manager_addFlight(managerPtr, 0, 1, 2));
    assert(manager_queryFlight(managerPtr, 0) == 1);
    assert(manager_queryFlightPrice(managerPtr, 0) == 2);
    assert(manager_addFlight(managerPtr, 0, 1, -1));
    assert(manager_queryFlight(managerPtr, 0) == 2);
    assert(manager_reserveFlight(managerPtr, 0, 0));
    assert(manager_queryFlight(managerPtr, 0) == 1);
    assert(manager_addFlight(managerPtr, 0, 1, 1));
    assert(manager_queryFlightPrice(managerPtr, 0) == 1);
    assert(manager_deleteCustomer(managerPtr, 0));
    assert(manager_queryFlight(managerPtr, 0) == 3);
    assert(manager_deleteFlight(managerPtr, 0));

    /* Test query interface for customer bill */

    assert(manager_queryCustomerBill(managerPtr, 0) == -1); /* does not exist */
    assert(manager_addCustomer(managerPtr, 0));
    assert(manager_queryCustomerBill(managerPtr, 0) == 0);
    assert(manager_addCar(managerPtr, 0, 1, 1));
    assert(manager_addRoom(managerPtr, 0, 1, 2));
    assert(manager_addFlight(managerPtr, 0, 1, 3));

    assert(manager_reserveCar(managerPtr, 0, 0));
    assert(manager_queryCustomerBill(managerPtr, 0) == 1);
    assert(!manager_reserveCar(managerPtr, 0, 0));
    assert(manager_queryCustomerBill(managerPtr, 0) == 1);
    assert(manager_addCar(managerPtr, 0, 0, 2));
    assert(manager_queryCar(managerPtr, 0) == 0);
    assert(manager_queryCustomerBill(managerPtr, 0) == 1);

    assert(manager_reserveRoom(managerPtr, 0, 0));
    assert(manager_queryCustomerBill(managerPtr, 0) == 3);
    assert(!manager_reserveRoom(managerPtr, 0, 0));
    assert(manager_queryCustomerBill(managerPtr, 0) == 3);
    assert(manager_addRoom(managerPtr, 0, 0, 2));
    assert(manager_queryRoom(managerPtr, 0) == 0);
    assert(manager_queryCustomerBill(managerPtr, 0) == 3);

    assert(manager_reserveFlight(managerPtr, 0, 0));
    assert(manager_queryCustomerBill(managerPtr, 0) == 6);
    assert(!manager_reserveFlight(managerPtr, 0, 0));
    assert(manager_queryCustomerBill(managerPtr, 0) == 6);
    assert(manager_addFlight(managerPtr, 0, 0, 2));
    assert(manager_queryFlight(managerPtr, 0) == 0);
    assert(manager_queryCustomerBill(managerPtr, 0) == 6);

    assert(manager_deleteCustomer(managerPtr, 0));
    assert(manager_deleteCar(managerPtr, 0, 1));
    assert(manager_deleteRoom(managerPtr, 0, 1));
    assert(manager_deleteFlight(managerPtr, 0));

   /* Test reservation interface */

    assert(manager_addCustomer(managerPtr, 0));
    assert(manager_queryCustomerBill(managerPtr, 0) == 0);
    assert(manager_addCar(managerPtr, 0, 1, 1));
    assert(manager_addRoom(managerPtr, 0, 1, 2));
    assert(manager_addFlight(managerPtr, 0, 1, 3));

    assert(!manager_cancelCar(managerPtr, 0, 0)); /* do not have reservation */
    assert(manager_reserveCar(managerPtr, 0, 0));
    assert(manager_queryCar(managerPtr, 0) == 0);
    assert(manager_cancelCar(managerPtr, 0, 0));
    assert(manager_queryCar(managerPtr, 0) == 1);

    assert(!manager_cancelRoom(managerPtr, 0, 0)); /* do not have reservation */
    assert(manager_reserveRoom(managerPtr, 0, 0));
    assert(manager_queryRoom(managerPtr, 0) == 0);
    assert(manager_cancelRoom(managerPtr, 0, 0));
    assert(manager_queryRoom(managerPtr, 0) == 1);

    assert(!manager_cancelFlight(managerPtr, 0, 0)); /* do not have reservation */
    assert(manager_reserveFlight(managerPtr, 0, 0));
    assert(manager_queryFlight(managerPtr, 0) == 0);
    assert(manager_cancelFlight(managerPtr, 0, 0));
    assert(manager_queryFlight(managerPtr, 0) == 1);

    assert(manager_deleteCar(managerPtr, 0, 1));
    assert(manager_deleteRoom(managerPtr, 0, 1));
    assert(manager_deleteFlight(managerPtr, 0));
    assert(manager_deleteCustomer(managerPtr, 0));

    manager_free(managerPtr);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_MANAGER */


/* =============================================================================
 *
 * End of manager.c
 *
 * =============================================================================
 */
