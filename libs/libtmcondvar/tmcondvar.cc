#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <cassert>
#include "tm_support.h"
#include "tmcondvar.h"

/// We might want to track statistics
#ifdef DEBUG
/// When statis is on, we get 5 counters
struct stats_t {
    int waits, broadcasts, signals, wakeups, empties;
    void inc_waits() { waits++; }
    void inc_broadcasts() { broadcasts++; }
    void inc_signals() { signals++; }
    void inc_wakeups() { wakeups++; }
    void inc_empties() { empties++; }
};
#else
/// When stats is off, we get no-ops
struct stats_t {
    void inc_waits() { }
    void inc_broadcasts() { }
    void inc_signals() { }
    void inc_wakeups() { }
    void inc_empties() { }
};
#endif

/// A tmcondvar is a doubly-linked list, where entries in the list are
/// per-thread semaphores.  This struct is a node in the list.
struct node_t {
    sem_t   semaphore; /// The thread's semaphore
    node_t* next;      /// next pointer
    node_t* prev;      /// previous pointer

    /// optional diagnostics/statistics
    stats_t stats;

    /// At construction time, the node is not in a list, but the semaphore is
    /// valid
    node_t()
        : next(NULL), prev(NULL)
    {
        sem_init(&semaphore, 0, 0);
    }
};

/// Our condvar implementation requires that each thread has a thread-local
/// sem_node_t object that contains a properly configured semaphore.  We
/// don't want the semaphore to be visible outside of this library, so we
/// hide it in an anonymous namespace.
namespace {
  thread_local node_t* my_node = NULL;
}

/// Here we define the condvar type.  It's just a list.  Note, too, that
/// we've typedef'd it to tmcondvar_t.
struct tmcondvar {
    /// The head of the queue of semaphores.  Dequeue happens from here
    node_t* head;

    /// The tail of the queue of semaphores.  Enqueue happens from here
    node_t* tail;

    /// Constructor for a condvar just makes sure the list is empty
    tmcondvar() : head(NULL), tail(NULL) { }
};

/// Helper functions
namespace {
  /// If the current code is transactional, then register a commit handler;
  /// otherwise run the commit handler immediately
  ///
  /// The handler should be a function that takes a single void* and returns
  /// an int.  The parameter to pass to the function should be given as
  /// /arg/.
  __attribute__((transaction_pure))
  void register_handler(int (*func)(void*), void* arg) {
      // Currently, _ITM_inTransaction() does not work from HTM... it just
      // aborts the transaction and falls back to STM mode.  For details, see
      // libitm/query.c.  Since register_handler() is always called from a
      // transaction inside of this file, we can just add the onCommit
      // handler directly.
      //
      // TODO: verify that the userCommitActions are run when a GCC HTM
      // transaction is used.  If not, we'll need a warning to remind users
      // that our supplied version of GCC's HTM is required.
      //
      // NB: for reference, we could use:
      //
      //   if(_ITM_inTransaction() == outsideTransaction)
      //
      // to check the current state
      _ITM_addUserCommitAction((_ITM_userCommitFunction)func,
                               _ITM_noTransactionId, arg);
  }

  /// Helper function; given a parameter that is a sem_node_t, this will
  /// iterate through the list of semaphores rooted at that head, and signal
  /// each.
  int broadcast_iterate(void* hd) {
      node_t* head = (node_t*)hd;
      while (head) {
          // NB: must read the head, then set its next to null, then signal
          //     the semaphore, or else we can race with subsequent uses of
          //     the sem_node_t by its owning thread.
          node_t* sn = head;
          head = head->next;

          // probably not necessary, but it doesn't hurt to make the ordering
          // explicit
          atomic_thread_fence (std::memory_order_seq_cst);

          sem_post(&sn->semaphore);
          my_node->stats.inc_wakeups();
      }
  }
}

/// Since we have a C interface to tmcondvars, we need a C-friendly way of
/// calling the constructor and returning the result
tmcondvar_t* tmcondvar_create() {
    return new tmcondvar();
}

/// Initialize a thread by creating its list node (and allocating a
/// semaphore)
void tmcondvar_thread_init(void) {
    my_node = new node_t();
}

/// Perform a cond_wait by adding the thread's node to the queue and then
/// waiting on the thread's semaphore.
///
/// NB: the wait is going to happen at commit time.  We're in big trouble if
/// the transaction doesn't commit immediately after calling this!
__attribute__((transaction_safe))
void tmcondvar_wait(tmcondvar_t* cv) {
    // make sure the node is disconnected
    my_node->next = NULL;
    my_node->prev = NULL;

    // Invariant: my_node->semaphore has a count of zero
    //
    // NB: Just to be safe, we'll use a transaction when inserting the node into the list
    __transaction_atomic {
        // add node to tail
        if (cv->tail == NULL && cv->head == NULL) {
            cv->head = cv->tail = my_node; // empty list
        }
        else {
            my_node->prev = cv->tail;
            cv->tail->next = my_node;
            cv->tail = my_node;
        }
        // set a handler to wait when the outermost transaction commits
        register_handler((int (*)(void*))sem_wait,
                         static_cast<void*>(&my_node->semaphore));
        my_node->stats.inc_waits();
    }
}

/// Perform a cond_signal by dequeueing a semaphore, and then registering a
/// commit handler to wake a thread on the semaphore, if such a thread
/// exists.
///
/// Note: the waking won't happen until the transaction commits.
__attribute__((transaction_safe))
void tmcondvar_signal(tmcondvar_t* cv) {
    // again, to be safe, use a transaction when interacting with the list
    __transaction_atomic {
        // if the queue is empty, return early
        node_t* sn = cv->head;
        if (sn == NULL)
            return;

        // remove node from head
        if (cv->head == cv->tail) {
            cv->tail = cv->head = NULL; // only entry
        }
        else {
            cv->head = cv->head->next;
            cv->head->prev = NULL;
        }

        // register an oncommit handler to wake thread
        register_handler((int (*)(void*))sem_post,
                         static_cast<void*>(&sn->semaphore));
        my_node->stats.inc_signals();
    }
}

/// Do a cond_signal, but pull from the back of the list
__attribute__((transaction_safe))
void tmcondvar_signal_back(tmcondvar_t* cv) {
    __transaction_atomic {
        // if the queue is empty, return early
        node_t* sn = cv->tail;
        if (sn == NULL)
            return;

        // remove node from tail
        if (cv->head == cv->tail) {
            cv->tail = cv->head = NULL; // only entry
        }
        else {
            cv->tail = cv->tail->prev;
            cv->tail->next = NULL;
        }

        // register an oncommit handler to wake thread
        register_handler((int (*)(void*))sem_post,
                         static_cast<void*>(&sn->semaphore));
        my_node->stats.inc_signals();
    }
}

/// Perform a cond_broadcast by severing the queue, and then registering a
/// commit handler that will iterate through the entire (severed) queue and
/// signal all semaphores
__attribute__((transaction_safe))
void tmcondvar_broadcast(tmcondvar_t* cv) {
    __transaction_atomic {
        // If the list is empty, return
        node_t* h = cv->head;
        if (h == NULL) {
            my_node->stats.inc_empties();
            return;
        }

        // sever the list, set the handler to broadcast
        cv->head = cv->tail = NULL;
        register_handler(broadcast_iterate, (void*)h);
        my_node->stats.inc_broadcasts();
    }
}

/**
 *  Assuming the lock is held, this will enqueue the thread, release the
 *  lock, sleep on the semaphore, and then re-acquire the lock
 */
void tmcondvar_wait_lock(tmcondvar_t* cv, pthread_mutex_t* lock) {
    // make sure the node is disconnected
    my_node->next = NULL;
    my_node->prev = NULL;

    // Invariant: my_node->semaphore has a count of zero
    //
    // We assume caller has lock, and that's good enough when enqueueing

    // add node to tail
    if (cv->tail == NULL && cv->head == NULL) {
        cv->head = cv->tail = my_node; // empty list
    }
    else {
        my_node->prev = cv->tail;
        cv->tail->next = my_node;
        cv->tail = my_node;
    }

    // release the lock, then sleep on the semaphore, then reacquire the lock
    pthread_mutex_unlock(lock);
    sem_wait(&my_node->semaphore);
    my_node->stats.inc_waits();
    pthread_mutex_lock(lock);
}

/// Assuming the lock is held, this will dequeue one entry from the condvar
/// queue, wake that thread, and keep going.
void tmcondvar_signal_lock(tmcondvar_t* cv) {
    // if the queue is empty, return early
    node_t* sn = cv->head;
    if (sn == NULL)
        return;

    // remove node from head
    if (cv->head == cv->tail) {
        cv->tail = cv->head = NULL; // only entry
    }
    else {
        cv->head = cv->head->next;
        cv->head->prev = NULL;
    }

    // wake the thread
    sem_post(&sn->semaphore);
    my_node->stats.inc_signals();
}

/// Assuming the lock is held, dequeue from the back of the queue and wake a
/// thread
void tmcondvar_signal_back_lock(tmcondvar_t* cv) {
    // if the queue is empty, return early
    node_t* sn = cv->tail;
    if (sn == NULL)
        return;

    // remove node from tail
    if (cv->head == cv->tail) {
        cv->tail = cv->head = NULL; // only entry
    }
    else {
        cv->tail = cv->tail->prev;
        cv->tail->next = NULL;
    }

    // wake the thread
    sem_post(&sn->semaphore);
    my_node->stats.inc_signals();
}

/// Assuming the lock is held, this will dequeue all threads from the condvar
/// queue and wake them all
void tmcondvar_broadcast_lock(tmcondvar_t* cv) {
    node_t* h = cv->head;
    // If the list is empty, return
    if (h == NULL) {
        my_node->stats.inc_empties();
        return;
    }

    // sever the list and broadcast
    cv->head = cv->tail = NULL;
    broadcast_iterate((void*)h);
    my_node->stats.inc_broadcasts();
}
