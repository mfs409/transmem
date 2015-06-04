/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include "action.h"
#include "client.h"
#include "manager.h"
#include "reservation.h"
#include "thread.h"
#include "tm_transition.h"

/* =============================================================================
 * client_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
client_t::client_t(long _id,
                   manager_t* _managerPtr,
                   long _numOperation,
                   long _numQueryPerTransaction,
                   long _queryRange,
                   long _percentUser)
{
    id = _id;
    managerPtr = _managerPtr;
    randomPtr.seed(id);
    numOperation = _numOperation;
    numQueryPerTransaction = _numQueryPerTransaction;
    queryRange = _queryRange;
    percentUser = _percentUser;
}

/* =============================================================================
 * selectAction
 * =============================================================================
 */
static action_t
selectAction (long r, long percentUser)
{
    action_t action;

    if (r < percentUser) {
        action = ACTION_MAKE_RESERVATION;
    } else if (r & 1) {
        action = ACTION_DELETE_CUSTOMER;
    } else {
        action = ACTION_UPDATE_TABLES;
    }

    return action;
}


/* =============================================================================
 * client_run
 * -- Execute list operations on the database
 * =============================================================================
 */
void
client_run (void* argPtr)
{
    long myId = thread_getId();
    client_t* clientPtr = ((client_t**)argPtr)[myId];

    manager_t* managerPtr = clientPtr->managerPtr;
    std::mt19937&  randomPtr  = clientPtr->randomPtr;

    long numOperation           = clientPtr->numOperation;
    long numQueryPerTransaction = clientPtr->numQueryPerTransaction;
    long queryRange             = clientPtr->queryRange;
    long percentUser            = clientPtr->percentUser;

    long types[numQueryPerTransaction];
    long ids[numQueryPerTransaction];
    long ops[numQueryPerTransaction];
    long prices[numQueryPerTransaction];

    for (long i = 0; i < numOperation; i++) {
        long r = randomPtr() % 100;
        action_t action = selectAction(r, percentUser);

        switch (action) {
            case ACTION_MAKE_RESERVATION: {
                long maxPrices[NUM_RESERVATION_TYPE] = { -1, -1, -1 };
                long maxIds[NUM_RESERVATION_TYPE] = { -1, -1, -1 };
                long n;
                long numQuery = randomPtr() % numQueryPerTransaction + 1;
                long customerId = randomPtr() % queryRange + 1;
                for (n = 0; n < numQuery; n++) {
                    types[n] = randomPtr() % NUM_RESERVATION_TYPE;
                    ids[n] = (randomPtr() % queryRange) + 1;
                }
                bool isFound = false;
                bool done = true;
                //[wer210] I modified here to remove _ITM_abortTransaction().
                while (1) {
                  __transaction_atomic {
                    for (n = 0; n < numQuery; n++) {
                      long t = types[n];
                      long id = ids[n];
                      long price = -1;
                      switch (t) {
                       case RESERVATION_CAR:
                        if (manager_queryCar(managerPtr, id) >= 0) {
                          price = manager_queryCarPrice(managerPtr, id);
                        }
                        break;
                       case RESERVATION_FLIGHT:
                        if (manager_queryFlight(managerPtr, id) >= 0) {
                          price = manager_queryFlightPrice(managerPtr, id);
                        }
                        break;
                       case RESERVATION_ROOM:
                        if (manager_queryRoom(managerPtr, id) >= 0) {
                          price = manager_queryRoomPrice(managerPtr, id);
                        }
                        break;
                       default:
                        assert(0);
                      }
                      //[wer210] read-only above
                      if (price > maxPrices[t]) {
                        maxPrices[t] = price;
                        maxIds[t] = id;
                        isFound = true;
                      }
                    } /* for n */

                    if (isFound) {
                      done = done && manager_addCustomer(managerPtr, customerId);
                    }

                    if (maxIds[RESERVATION_CAR] > 0) {
                      done = done && manager_reserveCar(managerPtr,
                                                  customerId, maxIds[RESERVATION_CAR]);
                    }

                    if (maxIds[RESERVATION_FLIGHT] > 0) {
                      done = done && manager_reserveFlight(managerPtr,
                                                     customerId, maxIds[RESERVATION_FLIGHT]);
                    }
                    if (maxIds[RESERVATION_ROOM] > 0) {
                      done = done && manager_reserveRoom(managerPtr,
                                                   customerId, maxIds[RESERVATION_ROOM]);
                    }
                    if (done) break;
                    else __transaction_cancel;
                  } // TM_END
            }
                break;

            }

            case ACTION_DELETE_CUSTOMER: {
                long customerId = randomPtr() % queryRange + 1;
                bool done = true;
                while (1) {
                  __transaction_atomic {
                    long bill = manager_queryCustomerBill(managerPtr, customerId);
                    if (bill >= 0) {
                      done = done && manager_deleteCustomer(managerPtr, customerId);
                    }
                    if(done) break;
                    else __transaction_cancel;
                  }
                }
                break;
            }

            case ACTION_UPDATE_TABLES: {
                long numUpdate = randomPtr() % numQueryPerTransaction + 1;
                long n;
                for (n = 0; n < numUpdate; n++) {
                    types[n] = randomPtr() % NUM_RESERVATION_TYPE;
                    ids[n] = (randomPtr() % queryRange) + 1;
                    ops[n] = randomPtr() % 2;
                    if (ops[n]) {
                        prices[n] = ((randomPtr() % 5) * 10) + 50;
                    }
                }
                bool done = true;
                while (1) {
                  __transaction_atomic {
                    for (n = 0; n < numUpdate; n++) {
                      long t = types[n];
                      long id = ids[n];
                      long doAdd = ops[n];
                      if (doAdd) {
                        long newPrice = prices[n];
                        switch (t) {
                         case RESERVATION_CAR:
                          done = done && manager_addCar(managerPtr, id, 100, newPrice);
                          break;
                         case RESERVATION_FLIGHT:
                          done = done && manager_addFlight(managerPtr, id, 100, newPrice);
                          break;
                         case RESERVATION_ROOM:
                          done = done && manager_addRoom(managerPtr, id, 100, newPrice);
                          break;
                         default:
                          assert(0);
                        }
                      } else { /* do delete */
                        switch (t) {
                         case RESERVATION_CAR:
                          done = done && manager_deleteCar(managerPtr, id, 100);
                          break;
                         case RESERVATION_FLIGHT:
                          done = done && manager_deleteFlight(managerPtr, id);
                          break;
                         case RESERVATION_ROOM:
                          done = done && manager_deleteRoom(managerPtr, id, 100);
                          break;
                         default:
                          assert(0);
                        }
                      }
                    }
                  if (done) break;
                  else __transaction_cancel;
                  } // TM_END
                }
                break;
            }

            default:
                assert(0);

        } /* switch (action) */

    } /* for i */

}
