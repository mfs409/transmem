#ifndef CONDVAR_H__
#define CONDVAR_H__

#include <semaphore.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  [mfs] We should consider having a more opaque interface, such as:
 *
 *    struct tm_cond_t;
 *    void   tm_cond_init(tm_cond_t* restrict);
 *    void   tm_cond_register_thread();
 *    void   tm_cond_wait(tm_cond_t* restrict);
 *    void   tm_cond_signal(tm_cond_t* restrict);
 *    void   tm_cond_broadcast(tm_cond_t* restrict);
 *
 *  Another open question is whether we should consider having
 *  register_thread at all... we could always do a test within each
 *  cond_wait, and use a /pure/ function if necessary.
 */

/**
 *  This class implements a three-function API for condition synchronization.
 *
 *  The three functions are cond_wait, cond_signal and cond_broadcast.
 *  Unlike pthread_cond_t, these implementations do not have any relationship
 *  to mutexes, and instead should be used inside of transactions.
 *
 *  Note that unlike pthread_cond_t, this condition variable implementation
 *  does not suffer from spurious wakeups.
 *
 *  The algorithm relies on each thread having its own semaphore, and
 *  performing wait() by enqueueing its semaphore on a per-condvar queue.  To
 *  this end, there is a fourth function in the API, thread_init, which we
 *  use to register a thread so that it can safely use condvars (i.e., so
 *  that it has a semaphore).
 */
typedef struct cv_sem_node_t
{
    /**
     *  The semaphore
     */
    sem_t semaphore;

    /**
     *  The next pointer
     */
    struct cv_sem_node_t* next;

    /**
     *  The prev pointer
     */
    struct cv_sem_node_t* prev;
#ifdef DEBUG
    /**
     *  Diagnostics
     */
    int waits;
    int broadcasts;
    int signals;
    int wakeups;
    int empties;
#endif

} cv_sem_node_t;

void cv_init_sem_node (cv_sem_node_t* sem_node);

typedef struct cv_cond_var_t
{
    /**
     *  The head of the queue of semaphores.  Dequeue happens from here
     */
    cv_sem_node_t* head;

    /**
     *  The tail of the queue of semaphores.  Enqueue happens from here
     */
    cv_sem_node_t* tail;

} cv_cond_var_t;

/**
*  Register a thread so that it can use transactional condvars
*/
void cv_thread_init (void);

/**
*  Initialize a condvar
*/
void cv_init_cond_var (cv_cond_var_t* cv);

/**
*  Wait on this condvar.  See pthread_cond_wait().
*  NB: lock parameter is for legacy lock-based code.
*      when used alone or inside transactions, this 
*      parameter can be ommited.
*/
__attribute__((transaction_safe))
void cv_cond_wait (cv_cond_var_t* cv, pthread_mutex_t* lock);

/**
*  Wake a thread waiting on this condvar.  See pthread_cond_signal().
*/
__attribute__((transaction_safe))
void cv_cond_signal (cv_cond_var_t* cv);

/**
*  Wake a thread waiting on this condvar from the back of the queue.  See pthread_cond_signal().
*/
__attribute__((transaction_safe))
void cv_cond_signal_back (cv_cond_var_t* cv);

/**
*  Wake all threads waiting on a condvar.  See pthread_cond_broadcast().
*/
__attribute__((transaction_safe))
void cv_cond_broadcast (cv_cond_var_t* cv);

/**
*  A lock-based interface to this condvar mechanism:  here is the wait code
*/
void cv_lock_cond_wait (cv_cond_var_t* cv, pthread_mutex_t* lock);

/**
*  A lock-based interface to this condvar mechanism: here is the signal code
*/
void cv_lock_cond_signal (cv_cond_var_t* cv);

/**
*  A lock-based interface to this condvar mechanism: here is the signal code
*/
void cv_lock_cond_signal_back (cv_cond_var_t* cv);

/**
*  A lock-based interaface to this condvar mechanism: here is the
*  broadcast code
*/
void cv_lock_cond_broadcast (cv_cond_var_t* cv);

#ifdef __cplusplus
}
#endif
#endif
