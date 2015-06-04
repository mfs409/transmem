/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * manager.c: Travel reservation resource manager
 */

#include <cassert>
#include "manager.h"
#include "tm_transition.h"

/* =============================================================================
 * DECLARATION OF TM_SAFE FUNCTIONS
 * =============================================================================
 */
__attribute__((transaction_safe))
long queryNumFree(std::map<long, reservation_t*>* tbl, long id);

__attribute__((transaction_safe))
long queryPrice(std::map<long, reservation_t*>* tbl, long id);

__attribute__((transaction_safe))
bool reserve(std::map<long, reservation_t*>* tbl, std::map<long, customer_t*>* custs,
             long customerId, long id, reservation_type_t type);

__attribute__((transaction_safe))
bool cancel(std::map<long, reservation_t*>* tbl, std::map<long, customer_t*>* custs,
            long customerId, long id, reservation_type_t type);

__attribute__((transaction_safe))
bool addReservation(std::map<long, reservation_t*>* tbl, long id, long num, long price);

/**
 * Constructor for manager objects
 */
manager_t::manager_t()
{
    carTable = new std::map<long, reservation_t*>();
    roomTable = new std::map<long, reservation_t*>();
    flightTable = new std::map<long, reservation_t*>();
    customerTable = new std::map<long, customer_t*>();
}

/**
 * Destructor
 *
 * [mfs] notes in the earlier code suggest that contents are not deleted
 *       here.  That's bad, but we can't fix it yet.
 */
manager_t::~manager_t()
{
    delete carTable;
    delete roomTable;
    delete flightTable;
    delete customerTable;
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
addReservation (std::map<long, reservation_t*>* tbl, long id, long num, long price)
{
    auto reservation = tbl->find(id);

    bool success = true;
    if (reservation == tbl->end()) {
        /* Create new reservation */
        if (num < 1 || price < 0) {
          //return FALSE;
          return true;
        }

        //[wer210] there was aborts inside RESERVATION_ALLOC, passing an extra parameter.
        reservation_t* reservationPtr = new reservation_t(id, num, price, &success);
        if (!success) return false;

        assert(reservationPtr != NULL);
        tbl->insert(std::make_pair(id, reservationPtr));
    } else {
      /* Update existing reservation */
      //[wer210] there was aborts inside RESERVATION_ADD_TO_TOTAL, passing an extra parameter.
      if (!reservation->second->addToTotal(num, &success)) {
        //return FALSE;
        if (success)
          return true;
        else return false;
      }

      if (reservation->second->numTotal == 0) {
        int numremoved = tbl->erase(id);
        bool status = numremoved != 0;
        if (status == false) {
          //_ITM_abortTransaction(2);
          return false;
        }

        delete reservation->second;
      } else {
        //[wer210] there was aborts inside RESERVATIOn_UPDATE_PRICE, and return was not used
        if (!reservation->second->updatePrice(price))
          return false;
      }
    }

    return true;
}


/* =============================================================================
 * manager_t::addCar
 * -- Add cars to a city
 * -- Adding to an existing car overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Return value not used before.
__attribute__((transaction_safe)) bool
manager_t::addCar (long carId, long numCars, long price)
{
    return addReservation(carTable, carId, numCars, price);
}


/* =============================================================================
 * manager_t::deleteCar
 * -- Delete cars from a city
 * -- Decreases available car count (those not allocated to a customer)
 * -- Fails if would make available car count negative
 * -- If decresed to 0, deletes entire entry
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::deleteCar (long carId, long numCar)
{
    /* -1 keeps old price */
    return addReservation(carTable, carId, -numCar, -1);
}


/* =============================================================================
 * manager_t::addRoom
 * -- Add rooms to a city
 * -- Adding to an existing room overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::addRoom (long roomId, long numRoom, long price)
{
    return addReservation(roomTable, roomId, numRoom, price);
}



/* =============================================================================
 * manager_t::deleteRoom
 * -- Delete rooms from a city
 * -- Decreases available room count (those not allocated to a customer)
 * -- Fails if would make available room count negative
 * -- If decresed to 0, deletes entire entry
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::deleteRoom (long roomId, long numRoom)
{
    /* -1 keeps old price */
    return addReservation(roomTable, roomId, -numRoom, -1);
}


/* =============================================================================
 * manager_t::addFlight
 * -- Add seats to a flight
 * -- Adding to an existing flight overwrite the price if 'price' >= 0
 * -- Returns TRUE on success, FALSE on failure
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::addFlight (long flightId, long numSeat, long price)
{
    return addReservation(flightTable, flightId, numSeat, price);
}



/* =============================================================================
 * manager_t::deleteFlight
 * -- Delete an entire flight
 * -- Fails if customer has reservation on this flight
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] return not used before, make it to indicate aborts
__attribute__((transaction_safe)) bool
manager_t::deleteFlight (long flightId)
{
    auto res = flightTable->find(flightId);

    if (res == flightTable->end()) {
        return true;
    }

    if (res->second->numUsed > 0) {
      //return FALSE; /* somebody has a reservation */
      return true;
    }

    return addReservation(flightTable,
                          flightId,
                          -1*res->second->numTotal,
                          -1 /* -1 keeps old price */);
}


/* =============================================================================
 * manager_t::addCustomer
 * -- If customer already exists, returns failure
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Function is called inside a transaction in client.c
//         But return value of this function was never used,
//         so I make it to return true all the time except when need to abort.
__attribute__((transaction_safe)) bool
manager_t::addCustomer (long customerId)
{
    auto res = customerTable->find(customerId);

    if (res != customerTable->end()) {
        return true;
    }

    customer_t* customerPtr = new customer_t(customerId);
    assert(customerPtr != NULL);

    bool status = customerTable->insert(std::make_pair(customerId, customerPtr)).second;
    if (status == false) {
      //_ITM_abortTransaction(2);
      return false;
    }

    return true;
}



/* =============================================================================
 * manager_t::deleteCustomer
 * -- Delete this customer and associated reservations
 * -- If customer does not exist, returns success
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
//[wer210] Again, the return values were not used (except for test cases below)
//         So I make it alway returning true, unless need to abort a transaction.
__attribute__((transaction_safe)) bool
manager_t::deleteCustomer (long customerId)
{
    std::map<long, reservation_t*>* tables[NUM_RESERVATION_TYPE];
    auto res = customerTable->find(customerId);
    if (res == customerTable->end()) {
        return true;
    }

    tables[RESERVATION_CAR] = carTable;
    tables[RESERVATION_ROOM] = roomTable;
    tables[RESERVATION_FLIGHT] = flightTable;

    /* Cancel this customer's reservations */
    for (auto i : *res->second->reservationInfoList) {
        reservation_info_t* reservationInfoPtr = i;
        auto reservation = tables[i->type]->find(i->id);
        if (reservation == tables[i->type]->end()) {
            return false;
        }

      bool status = reservation->second->cancel();
      if (status == false) {
        //_ITM_abortTransaction(2);
        return false;
      }
      delete reservationInfoPtr;
    }

    int numerase = customerTable->erase(customerId);
    if (numerase == 0) {
        return false;
    }
    delete res->second;

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
long queryNumFree(std::map<long, reservation_t*>* tbl, long id)
{
    long numFree = -1;
    auto res = tbl->find(id);
    if (res != tbl->end()) {
        numFree = res->second->numFree;
    }
    return numFree;
}


/* =============================================================================
 * queryPrice
 * -- Return price of a reservation, -1 if failure
 * =============================================================================
 */
 __attribute__((transaction_safe))
long queryPrice(std::map<long, reservation_t*>* tbl, long id)
{
    long price = -1;
    auto res = tbl->find(id);
    if (res != tbl->end()) {
        price = res->second->price;
    }
    return price;
}


/* =============================================================================
 * manager_t::queryCar
 * -- Return the number of empty seats on a car
 * -- Returns -1 if the car does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_t::queryCar (long carId)
{
    return queryNumFree(carTable, carId);
}


/* =============================================================================
 * manager_t::queryCarPrice
 * -- Return the price of the car
 * -- Returns -1 if the car does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_t::queryCarPrice (long carId)
{
    return queryPrice(carTable, carId);
}


/* =============================================================================
 * manager_t::queryRoom
 * -- Return the number of empty seats on a room
 * -- Returns -1 if the room does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_t::queryRoom (long roomId)
{
    return queryNumFree(roomTable, roomId);
}


/* =============================================================================
 * manager_t::queryRoomPrice
 * -- Return the price of the room
 * -- Returns -1 if the room does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_t::queryRoomPrice (long roomId)
{
    return queryPrice(roomTable, roomId);
}


/* =============================================================================
 * manager_t::queryFlight
 * -- Return the number of empty seats on a flight
 * -- Returns -1 if the flight does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_t::queryFlight (long flightId)
{
    return queryNumFree(flightTable, flightId);
}


/* =============================================================================
 * manager_t::queryFlightPrice
 * -- Return the price of the flight
 * -- Returns -1 if the flight does not exist
 * =============================================================================
 */
__attribute__((transaction_safe)) long
manager_t::queryFlightPrice (long flightId)
{
    return queryPrice(flightTable, flightId);
}


/* =============================================================================
 * manager_t::queryCustomerBill
 * -- Return the total price of all reservations held for a customer
 * -- Returns -1 if the customer does not exist
 * =============================================================================
 */
__attribute__((transaction_safe))
long manager_t::queryCustomerBill(long customerId)
{
    long bill = -1;

    auto res = customerTable->find(customerId);
    if (res != customerTable->end()) {
        bill = res->second->getBill();
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
bool reserve(std::map<long, reservation_t*>* tbl, std::map<long, customer_t*>* custs,
             long customerId, long id, reservation_type_t type)
{
    auto cust = custs->find(customerId);
    if (cust == custs->end()) {
        return true;
    }
    customer_t* customerPtr = cust->second;

    auto res = tbl->find(id);
    if (res == tbl->end()) {
        return true;
    }
    reservation_t* reservationPtr = res->second;

    if (!reservationPtr->make()) {
      //return FALSE;
      return true;
    }

    if (!customerPtr->addReservationInfo(type, id, reservationPtr->price)) {
      /* Undo previous successful reservation */
      bool status = reservationPtr->cancel();
      if (status == false) {
          //_ITM_abortTransaction(2);
          return false;
      }
      //return FALSE;
    }
    return true;
}


/* =============================================================================
 * manager_t::reserveCar
 * -- Returns failure if the car or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::reserveCar (long customerId, long carId)
{
    return reserve(carTable,
                   customerTable,
                   customerId,
                   carId,
                   RESERVATION_CAR);
}


/* =============================================================================
 * manager_t::reserveRoom
 * -- Returns failure if the room or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::reserveRoom (long customerId, long roomId)
{
    return reserve(roomTable,
                   customerTable,
                   customerId,
                   roomId,
                   RESERVATION_ROOM);
}


/* =============================================================================
 * manager_t::reserveFlight
 * -- Returns failure if the flight or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::reserveFlight (long customerId, long flightId)
{
    return reserve(flightTable,
                   customerTable,
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
bool cancel(std::map<long, reservation_t*>* tbl, std::map<long, customer_t*>* custs,
            long customerId, long id, reservation_type_t type)
{

    auto cust = custs->find(customerId);
    if (cust == custs->end()) {
        return false;
    }
    customer_t* customerPtr = cust->second;

    auto res = tbl->find(id);
    if (res == tbl->end()) {
        return false;
    }
    reservation_t* reservationPtr = res->second;

    if (!reservationPtr->cancel()) {
        return false;
    }

    if (!customerPtr->removeReservationInfo(type, id)) {
        /* Undo previous successful cancellation */
      bool status = reservationPtr->make();
      if (status == false) {
        //_ITM_abortTransaction(2);
        return false;
      }
      return false;
    }
    return true;
}


/* =============================================================================
 * manager_t::cancelCar
 * -- Returns failure if the car, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::cancelCar (long customerId, long carId)
{
    return cancel(carTable,
                  customerTable,
                  customerId,
                  carId,
                  RESERVATION_CAR);
}


/* =============================================================================
 * manager_t::cancelRoom
 * -- Returns failure if the room, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::cancelRoom (long customerId, long roomId)
{
    return cancel(roomTable,
                  customerTable,
                  customerId,
                  roomId,
                  RESERVATION_ROOM);
}



/* =============================================================================
 * manager_t::cancelFlight
 * -- Returns failure if the flight, reservation, or customer does not exist
 * -- Returns TRUE on success, else FALSE
 * =============================================================================
 */
__attribute__((transaction_safe)) bool
manager_t::cancelFlight (long customerId, long flightId)
{
    return cancel(flightTable,
                  customerTable,
                  customerId,
                  flightId,
                  RESERVATION_FLIGHT);
}


/* =============================================================================
 * TEST_MANAGER
 * =============================================================================
 */
#ifdef TEST_MANAGER


#include <cassert>
#include <cstdio>


int
main ()
{
    manager_t::t* managerPtr;

    assert(memory_init(1, 4, 2));

    puts("Starting...");

    managerPtr = manager_t::alloc();

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
