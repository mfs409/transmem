/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * manager.h: Travel reservation resource manager
 */

#pragma once

#include <map>
#include "reservation.h"
#include "customer.h"

struct manager_t {
  std::map<long, reservation_t*>* carTable;
  std::map<long, reservation_t*>* roomTable;
  std::map<long, reservation_t*>* flightTable;
  std::map<long, customer_t*>* customerTable;

  manager_t();
  ~manager_t();

  /*
   * ADMINISTRATIVE INTERFACE
   */

  /*
   * addCar
   * -- Add cars to a city
   * -- Adding to an existing car overwrite the price if 'price' >= 0
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool addCar(long carId, long numCar, long price);


  /*
   * deleteCar
   * -- Delete cars from a city
   * -- Decreases available car count (those not allocated to a customer)
   * -- Fails if would make available car count negative
   * -- If decresed to 0, deletes entire entry
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool deleteCar(long carId, long numCar);


  /*
   * addRoom
   * -- Add rooms to a city
   * -- Adding to an existing room overwrites the price if 'price' >= 0
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool addRoom(long roomId, long numRoom, long price);


  /*
   * deleteRoom
   * -- Delete rooms from a city
   * -- Decreases available room count (those not allocated to a customer)
   * -- Fails if would make available room count negative
   * -- If decresed to 0, deletes entire entry
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool deleteRoom(long roomId, long numRoom);


  /*
   * addFlight
   * -- Add seats to a flight
   * -- Adding to an existing flight overwrites the price if 'price' >= 0
   * -- Returns TRUE on success, FALSE on failure
   */
  __attribute__((transaction_safe))
  bool addFlight(long flightId, long numSeat, long price);

  /*
   * deleteFlight
   * -- Delete an entire flight
   * -- Fails if customer has reservation on this flight
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool deleteFlight(long flightId);


  /*
   * addCustomer
   * -- If customer already exists, returns success
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool addCustomer(long customerId);


  /*
   * deleteCustomer
   * -- Delete this customer and associated reservations
   * -- If customer does not exist, returns success
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool deleteCustomer(long customerId);


  /*
   * QUERY INTERFACE
   */


  /*
   * queryCar
   * -- Return the number of empty seats on a car
   * -- Returns -1 if the car does not exist
   */
  __attribute__((transaction_safe))
  long queryCar(long carId);


  /*
   * queryCarPrice
   * -- Return the price of the car
   * -- Returns -1 if the car does not exist
   */
  __attribute__((transaction_safe))
  long queryCarPrice(long carId);


  /*
   * queryRoom
   * -- Return the number of empty seats on a room
   * -- Returns -1 if the room does not exist
   */
  __attribute__((transaction_safe))
  long queryRoom(long roomId);


  /*
   * queryRoomPrice
   * -- Return the price of the room
   * -- Returns -1 if the room does not exist
   */
  __attribute__((transaction_safe))
  long queryRoomPrice(long roomId);


  /*
   * queryFlight
   * -- Return the number of empty seats on a flight
   * -- Returns -1 if the flight does not exist
   */
  __attribute__((transaction_safe))
  long queryFlight(long flightId);


  /*
   * queryFlightPrice
   * -- Return the price of the flight
   * -- Returns -1 if the flight does not exist
   */
  __attribute__((transaction_safe))
  long queryFlightPrice(long flightId);


  /*
   * queryCustomerBill
   * -- Return the total price of all reservations held for a customer
   * -- Returns -1 if the customer does not exist
   */
  __attribute__((transaction_safe))
  long queryCustomerBill(long customerId);


  /*
   * RESERVATION INTERFACE
   */


  /*
   * reserveCar
   * -- Returns failure if the car or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool reserveCar(long customerId, long carId);


  /*
   * reserveRoom
   * -- Returns failure if the room or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool reserveRoom(long customerId, long roomId);


  /*
   * reserveFlight
   * -- Returns failure if the flight or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool reserveFlight(long customerId, long flightId);


  /*
   * cancelCar
   * -- Returns failure if the car, reservation, or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool cancelCar(long customerId, long carId);


  /*
   * cancelRoom
   * -- Returns failure if the room, reservation, or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool cancelRoom(long customerId, long roomId);


  /*
   * cancelFlight
   * -- Returns failure if the flight, reservation, or customer does not exist
   * -- Returns TRUE on success, else FALSE
   */
  __attribute__((transaction_safe))
  bool cancelFlight(long customerId, long flightId);
};
