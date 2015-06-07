#ifndef CONDVAR_HPP__
#define CONDVAR_HPP__

#include <semaphore.h>

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
 *  This class defines the node type for a list of semaphores.  It is only
 *  public because we currently are using a class-based interface, and one
 *  field of the class is of this type.
 */
struct sem_node_t
{
    /**
     *  The semaphore
     */
    sem_t semaphore;

    /**
     *  The next pointer
     */
    sem_node_t* next;

    /**
     *	The prev pointer
     */
    sem_node_t* prev;
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
    /**
     *  Constructor just initializes a semaphore
     */
    sem_node_t();
};

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
class cond_var_t
{
    /**
     *  The head of the queue of semaphores.  Dequeue happens from here
     */
    sem_node_t* head;

    /**
     *  The tail of the queue of semaphores.  Enqueue happens from here
     */
    sem_node_t* tail;

  public:

    /**
     *  Register a thread so that it can use transactional condvars
     */
    static void thread_init();

    /**
     *  Initialize a condvar
     */
    cond_var_t();

    /**
     *  Wait on this condvar.  See pthread_cond_wait().
     *  NB: lock parameter is for legacy lock-based code.
     *      when used alone or inside transactions, this 
     *      parameter can be ommited.
     */
    __attribute__((transaction_safe))
    void cond_wait(pthread_mutex_t* lock = 0);

    /**
     *  Wake a thread waiting on this condvar.  See pthread_cond_signal().
     */
    __attribute__((transaction_safe))
    void cond_signal();

    /**
     *  Wake a thread waiting on this condvar from the back of the queue.  See pthread_cond_signal().
     */
    __attribute__((transaction_safe))
    void cond_signal_back();
    
    /**
     *  Wake all threads waiting on a condvar.  See pthread_cond_broadcast().
     */
    __attribute__((transaction_safe))
    void cond_broadcast();

    /**
     *  A lock-based interface to this condvar mechanism:  here is the wait code
     */
    void lock_cond_wait(pthread_mutex_t* lock);

    /**
     *  A lock-based interface to this condvar mechanism: here is the signal code
     */
    void lock_cond_signal();

    /**
     *  A lock-based interface to this condvar mechanism: here is the signal code
     */
    void lock_cond_signal_back();
    
    /**
     *  A lock-based interaface to this condvar mechanism: here is the
     *  broadcast code
     */
    void lock_cond_broadcast();

};

#endif // CONDVAR_HPP__
