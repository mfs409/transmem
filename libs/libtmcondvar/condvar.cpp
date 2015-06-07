#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <pthread.h> // for new lock interface
#include <cassert>

#include "tm_support.h"
#include "condvar.hpp"
#include "condvar.h"

/**
 *  Our condvar implementation requires that each thread has a thread-local
 *  sem_node_t object that contains a properly configured semaphore.  We
 *  don't want the semaphore to be visible outside of this library, so we
 *  hide it in an anonymous namespace.
 */
namespace
{
  thread_local sem_node_t* my_semaphore = NULL;
  thread_local cv_sem_node_t* cv_my_semaphore = NULL;
}

/**
 *  If the current code is transactional, then register a commit handler;
 *  otherwise run the commit handler immediately
 */
namespace
{
  __attribute__((transaction_pure))
  void register_handler(int (*func)(void*), void* arg)
  {
/* current version of _ITM_inTransaction() do not support htm. whenever this
   function is called with htm transaction, it just aborts and fall back to
   stm mode. For details, see libitm/query.c
   Since in this condvar library we *ALWAYS* call this register_handler inside
   transaction scope, so we ellide this function call until later time when we
   have a correct _ITM_inTransaction support htm. This change will not affect
   the correctness of this condvar library.
*/
//      if(_ITM_inTransaction() == outsideTransaction)
//          func(arg);
//      else
          _ITM_addUserCommitAction((_ITM_userCommitFunction)func,
                                   _ITM_noTransactionId, arg);
  }
}

/**
 *  This is a diagnostic tool... if we can see all the threads' semaphores,
 *  it will make it easier to evaluate if they are working correctly or not.
 */
#if DEBUG
sem_node_t* all_semaphores[256];
std::atomic<int> next_slot(0);
#endif

namespace
{
  /**
   *  Helper function; given a parameter that is a sem_node_t, this will
   *  iterate through the list of semaphores rooted at that head, and signal
   *  each.
   */
  int cond_broadcast_iterate(void* hd)
  {
      sem_node_t* head = (sem_node_t*)hd;
      while (head) {
          // NB: must read the head, then set its next to null, then signal
          //     the semaphore, or else we can race with subsequent uses of
          //     the sem_node_t by its owning thread.
          sem_node_t* sn = head;
          head = head->next;

          // probably not necessary, but it doesn't hurt to make the ordering
          // explicit
          atomic_thread_fence(std::memory_order_seq_cst);

          sem_post(&sn->semaphore);
#ifdef DEBUG
          // DIAGNOSTICS
          my_semaphore->wakeups++;
#endif
      }
  }

  /**
   *  Helper function; given a parameter that is a sem_node_t, this will
   *  iterate through the list of semaphores rooted at that head, and signal
   *  each.
   */
  int cv_cond_broadcast_iterate (void* hd)
  {
      cv_sem_node_t* head = (cv_sem_node_t*)hd;
      while (head) {
          // NB: must read the head, then set its next to null, then signal
          //     the semaphore, or else we can race with subsequent uses of
          //     the sem_node_t by its owning thread.
          cv_sem_node_t* sn = head;
          head = head->next;

          // probably not necessary, but it doesn't hurt to make the ordering
          // explicit
          atomic_thread_fence (std::memory_order_seq_cst);

          sem_post(&sn->semaphore);
#ifdef DEBUG
          // DIAGNOSTICS
          cv_my_semaphore->wakeups++;
#endif
      }
  }

  /**
   *  Helper function; after a cond_wait commits, this code will actually
   *  perform the wait on the semaphore
   */
  int cond_wait_delayed(void* lock)
  {
#ifdef DEBUG
      // DIAGNOSTICS
      my_semaphore->waits++;
#endif
     // if lock is not null, it means that we are in the legacy lock-based code.
     // release the lock, then sleep on the semaphore, then reacquire the lock
      if(lock)
          pthread_mutex_unlock((pthread_mutex_t*)lock);
      sem_wait(&my_semaphore->semaphore);
      if(lock)
          pthread_mutex_lock((pthread_mutex_t*)lock);
  }

  /**
   *  Helper function; after a cond_wait commits, this code will actually
   *  perform the wait on the semaphore
   */
  int cv_cond_wait_delayed(void* lock)
  {
#ifdef DEBUG
      // DIAGNOSTICS
      cv_my_semaphore->waits++;
#endif
     // if lock is not null, it means that we are in the legacy lock-based code.
     // release the lock, then sleep on the semaphore, then reacquire the lock
      if(lock)
          pthread_mutex_unlock((pthread_mutex_t*)lock);
      sem_wait(&cv_my_semaphore->semaphore);
      if(lock)
          pthread_mutex_lock((pthread_mutex_t*)lock);
  }
}

/**
 *  Construct a sem_node_t.  Should only be called once per thread.
 */
#ifdef DEBUG
sem_node_t::sem_node_t()
    : next(NULL), prev(NULL), waits(0), broadcasts(0), signals(0), wakeups(0), empties(0)
{
    sem_init(&semaphore, 0, 0);
}
#else
sem_node_t::sem_node_t()
    : next(NULL), prev(NULL)
{
    sem_init(&semaphore, 0, 0);
}
#endif

void cv_init_sem_node (cv_sem_node_t* sem_node)
{
    sem_init(&sem_node->semaphore, 0, 0);
}

/**
 *  Construct a condvar by setting the head and tail of the queue to NULL
 */
cond_var_t::cond_var_t()
{
    head = tail = NULL;
}

void cv_init_cond_var (cv_cond_var_t* cv)
{
    cv->head = cv->tail = NULL;
}

/**
 *  Initialize a thread by giving it a sem_node object
 */
void cond_var_t::thread_init()
{
    my_semaphore = new sem_node_t();
#ifdef DEBUG
    int slot = next_slot.fetch_add(1);
    all_semaphores[slot] = my_semaphore;
#endif
}

void cv_thread_init (void)
{
    cv_my_semaphore = (cv_sem_node_t*)malloc(sizeof(cv_sem_node_t));
    cv_init_sem_node (cv_my_semaphore);
}
/**
 *  Perform a cond_wait by adding the thread's node to the queue and then
 *  waiting on the thread's semaphore.
 *
 *  Note: the wait is going to happen at commit time.  We're in big trouble
 *  if the transaction doesn't commit immediately after calling this!
 */
__attribute__((transaction_safe))
void cond_var_t::cond_wait(pthread_mutex_t* lock /* = NULL */)
{
    // make sure the node's next pointer is null
    my_semaphore->next = NULL;
    // make sure the node's prev pointer is null
    my_semaphore->prev = NULL;

    // [mfs] Invariant: my_semaphore->semaphore has a count of zero
//    __transaction_atomic {
        // enqueue my node, with special case for empty queue
        if (tail == NULL && head == NULL) {
            head = tail = my_semaphore;
        }
        else {
        my_semaphore->prev = tail;
            tail->next = my_semaphore;
            tail = my_semaphore;
        }
        // just in case this isn't the outermost transaction, register a
        // commit handler here.
    register_handler(cond_wait_delayed, (void*)lock);
//    }
}

__attribute__((transaction_safe))
void cv_cond_wait (cv_cond_var_t* cv, pthread_mutex_t* lock /* = NULL */)
{
    // make sure the node's next pointer is null
    cv_my_semaphore->next = NULL;
    // make sure the node's prev pointer is null
    cv_my_semaphore->prev = NULL;

    // [mfs] Invariant: my_semaphore->semaphore has a count of zero
//    __transaction_atomic {
        // enqueue my node, with special case for empty queue
        if (cv->tail == NULL && cv->head == NULL) {
            cv->head = cv->tail = cv_my_semaphore;
        }
        else {
        cv_my_semaphore->prev = cv->tail;
            cv->tail->next = cv_my_semaphore;
            cv->tail = cv_my_semaphore;
        }
        // just in case this isn't the outermost transaction, register a
        // commit handler here.
    register_handler(cv_cond_wait_delayed, (void*)lock);
//    }
}
/**
 *  Perform a cond_signal by dequeueing a semaphore, and then registering a
 *  commit handler to wake a thread on the semaphore, if such a thread
 *  exists.
 *
 *  Note: the waking won't happen until the transaction commits.
 */
__attribute__((transaction_safe))
void cond_var_t::cond_signal()
{
//    __transaction_atomic {
        // if the queue is empty, return early
        sem_node_t* sn = head;
        if (sn == NULL)
            return;

        // be sure to handle removing the last element in queue
        if (head == tail)
            tail = head = NULL;
        else
            head = head->next;

        // register an oncommit handler
        register_handler((int (*)(void*))sem_post,
                         static_cast<void*>(&sn->semaphore));
#ifdef DEBUG
        // DIAGNOSTICS
        my_semaphore->signals++;
#endif
//    }
}

__attribute__((transaction_safe))
void cv_cond_signal (cv_cond_var_t* cv)
{
//    __transaction_atomic {
        // if the queue is empty, return early
        cv_sem_node_t* sn = cv->head;
        if (sn == NULL)
            return;

        // be sure to handle removing the last element in queue
        if (cv->head == cv->tail)
            cv->tail = cv->head = NULL;
        else
            cv->head = cv->head->next;

        // register an oncommit handler
        register_handler((int (*)(void*))sem_post,
                         static_cast<void*>(&sn->semaphore));
#ifdef DEBUG
        // DIAGNOSTICS
        cv_my_semaphore->signals++;
#endif
//    }
}

/**
 *  Perform a cond_signal by stack-like popping out a semaphore, and
 *  then registering a commit handler to wake a thread on the semaphore,
 *  if such a thread exists.
 *  Note: the waking won't happen until the transaction commits.
 */
__attribute__((transaction_safe))
void cond_var_t::cond_signal_back()
{
    __transaction_atomic {
        // if the queue is empty, return early
        sem_node_t* sn = tail;
        if (sn == NULL)
            return;

        // be sure to handle removing the last element in queue
        if (head == tail)
            tail = head = NULL;
        else
            tail = tail->prev;

        // register an oncommit handler
        register_handler((int (*)(void*))sem_post,
                         static_cast<void*>(&sn->semaphore));
#ifdef DEBUG
        // DIAGNOSTICS
        my_semaphore->signals++;
#endif
    }
}

__attribute__((transaction_safe))
void cv_cond_signal_back (cv_cond_var_t* cv)
{
    __transaction_atomic {
        // if the queue is empty, return early
        cv_sem_node_t* sn = cv->tail;
        if (sn == NULL)
            return;

        // be sure to handle removing the last element in queue
        if (cv->head == cv->tail)
            cv->tail = cv->head = NULL;
        else
            cv->tail = cv->tail->prev;

        // register an oncommit handler
        register_handler((int (*)(void*))sem_post,
                         static_cast<void*>(&sn->semaphore));
#ifdef DEBUG
        // DIAGNOSTICS
        cv_my_semaphore->signals++;
#endif
    }
}
/**
 *  Perform a cond_broadcast by severing the queue, and then registering a
 *  commit handler that will iterate through the entire (severed) queue and
 *  signal all semaphores
 */
__attribute__((transaction_safe))
void cond_var_t::cond_broadcast()
{
    __transaction_atomic {
        sem_node_t* h = head;
#ifdef DEBUG
        if (h == NULL)
            my_semaphore->empties++;
#endif
        head = tail = NULL;
        register_handler(cond_broadcast_iterate, (void*)h);
#ifdef DEBUG
        // DIAGNOSTICS
        my_semaphore->broadcasts++;
#endif
    }
}

__attribute__((transaction_safe))
void cv_cond_broadcast (cv_cond_var_t* cv)
{
    __transaction_atomic {
        cv_sem_node_t* h = cv->head;
#ifdef DEBUG
        if (h == NULL)
            cv_my_semaphore->empties++;
#endif
        cv->head = cv->tail = NULL;
        register_handler(cv_cond_broadcast_iterate, (void*)h);
#ifdef DEBUG
        // DIAGNOSTICS
        cv_my_semaphore->broadcasts++;
#endif
    }
}

/**
 *  Assuming the lock is held, this will enqueue the thread, release the
 *  lock, sleep on the semaphore, and then re-acquire the lock
 */
void cond_var_t::lock_cond_wait(pthread_mutex_t* lock)
{
    // make sure the node's next pointer is null
    my_semaphore->next = NULL;
    // make sure the node's prev pointer is null
    my_semaphore->prev = NULL;

    // [mfs] Invariant: my_semaphore->semaphore has a count of zero

    // enqueue my node, with special case for empty queue
    if (tail == NULL && head == NULL) {
        head = tail = my_semaphore;
    }
    else {
    my_semaphore->prev = tail;
        tail->next = my_semaphore;
        tail = my_semaphore;
    }

    // release the lock, then sleep on the semaphore, then reacquire the lock
    pthread_mutex_unlock(lock);
    cond_wait_delayed(NULL);
    pthread_mutex_lock(lock);
}

void cv_lock_cond_wait (cv_cond_var_t* cv, pthread_mutex_t* lock)
{
    // make sure the node's next pointer is null
    cv_my_semaphore->next = NULL;
    // make sure the node's prev pointer is null
    cv_my_semaphore->prev = NULL;

    // [mfs] Invariant: my_semaphore->semaphore has a count of zero

    // enqueue my node, with special case for empty queue
    if (cv->tail == NULL && cv->head == NULL) {
        cv->head = cv->tail = cv_my_semaphore;
    }
    else {
    cv_my_semaphore->prev = cv->tail;
        cv->tail->next = cv_my_semaphore;
        cv->tail = cv_my_semaphore;
    }

    // release the lock, then sleep on the semaphore, then reacquire the lock
    pthread_mutex_unlock(lock);
    cv_cond_wait_delayed(NULL);
    pthread_mutex_lock(lock);
}

/**
 *  Assuming the lock is held, this will dequeue one entry from the condvar
 *  queue, wake that thread, and keep going.
 */
void cond_var_t::lock_cond_signal()
{
    // if the queue is empty, return early
    sem_node_t* sn = head;
    if (sn == NULL)
        return;

    // be sure to handle removing the last element in queue
    if (head == tail)
        tail = head = NULL;
    else
        head = head->next;

    // wake the thread
    sem_post(&sn->semaphore);
#ifdef DEBUG
    // DIAGNOSTICS
    my_semaphore->signals++;
#endif
}

void cv_lock_cond_signal (cv_cond_var_t* cv)
{
    // if the queue is empty, return early
    cv_sem_node_t* sn = cv->head;
    if (sn == NULL)
        return;

    // be sure to handle removing the last element in queue
    if (cv->head == cv->tail)
        cv->tail = cv->head = NULL;
    else
        cv->head = cv->head->next;

    // wake the thread
    sem_post(&sn->semaphore);
#ifdef DEBUG
    // DIAGNOSTICS
    cv_my_semaphore->signals++;
#endif
}
/**
 *  Assuming the lock is held, this will stack-like popping out one entry
 *  from the condvar queue, wake that thread, and keep going.
 */
void cond_var_t::lock_cond_signal_back()
{
    // if the queue is empty, return early
    sem_node_t* sn = tail;
    if (sn == NULL)
        return;

    // be sure to handle removing the last element in queue
    if (head == tail)
        tail = head = NULL;
    else
        tail = tail->prev;

    // wake the thread
    sem_post(&sn->semaphore);
#ifdef DEBUG
    // DIAGNOSTICS
    my_semaphore->signals++;
#endif
}

void cv_lock_cond_signal_back (cv_cond_var_t* cv)
{
    // if the queue is empty, return early
    cv_sem_node_t* sn = cv->tail;
    if (sn == NULL)
        return;

    // be sure to handle removing the last element in queue
    if (cv->head == cv->tail)
        cv->tail = cv->head = NULL;
    else
        cv->tail = cv->tail->prev;

    // wake the thread
    sem_post(&sn->semaphore);
#ifdef DEBUG
    // DIAGNOSTICS
    cv_my_semaphore->signals++;
#endif
}
/**
 *  Assuming the lock is held, this will dequeue all threads from the condvar
 *  queue and wake them all
 */
void cond_var_t::lock_cond_broadcast()
{
    sem_node_t* h = head;
#ifdef DEBUG
    if (h == NULL)
        my_semaphore->empties++;
#endif
    head = tail = NULL;

    cond_broadcast_iterate((void*)h);
#ifdef DEBUG
    // DIAGNOSTICS
    my_semaphore->broadcasts++;
#endif
}

void cv_lock_cond_broadcast (cv_cond_var_t* cv)
{
    cv_sem_node_t* h = cv->head;
#ifdef DEBUG
    if (h == NULL)
        my_semaphore->empties++;
#endif
    cv->head = cv->tail = NULL;

    cv_cond_broadcast_iterate((void*)h);
#ifdef DEBUG
    // DIAGNOSTICS
    cv_my_semaphore->broadcasts++;
#endif
}

