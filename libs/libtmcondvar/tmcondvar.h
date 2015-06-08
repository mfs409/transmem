#include <pthread.h>

#pragma once

/// The public interface to our transaction-friendly condition variables is a
/// C interface.  It mostly looks like a regular condvar, except that we
/// currently expose the need to do per-thread initialization, we have
/// mechanisms for signalling the front or back of the queue of waiting
/// threads, and we distinguish between TM and lock-based contexts.

#ifdef __cplusplus
extern "C" {
#endif

/// to clients, tmcondvar_t is an opaque type
struct tmcondvar;
typedef struct tmcondvar tmcondvar_t;

/// create and initialize a new tmcondvar
tmcondvar_t* tmcondvar_create();

/// Register a thread so that it can use transactional condvars
void tmcondvar_thread_init(void);

/// Wait on a condvar.  Behavior is similar to pthread_cond_wait(), except
/// that the call to tmcondvar_wait must be after the last shared memory
/// operation in the transaction.
__attribute__((transaction_safe))
void tmcondvar_wait(tmcondvar_t* cv);

/// Wake a thread waiting on this condvar.  Behavior is similar to
/// pthread_cond_signal(), except that we are explicit about the fact that
/// there is a queue of waiting threads, and thus this is guaranteed to wake
/// the *oldest* waiting thread.
///
/// NB: if you understand linearizability, you'll note that a program often
/// cannot tell who is the "oldest"
__attribute__((transaction_safe))
void tmcondvar_signal(tmcondvar_t* cv);

/// Wake a thread waiting on this condvar.  In this case, we will wake the
/// "newest" waiting thread, i.e., the one who called tmcondvar_wait() most
/// recently
__attribute__((transaction_safe))
void tmcondvar_signal_back(tmcondvar_t* cv);

/// Wake all threads waiting on this condvar.  Behavior is similar to
/// pthread_cond_broadcast()
__attribute__((transaction_safe))
void tmcondvar_broadcast(tmcondvar_t* cv);

/// When using tmcondvars from a lock-based context, this version of wait()
/// will release the lock, as in pthread_wait().
///
/// NB: We assume the lock is held when this call is made
void tmcondvar_wait_lock(tmcondvar_t* cv, pthread_mutex_t* lock);

/// Wake a thread waiting on this condvar; this variant is to be used when
/// condvars are accessed from a lock-based context.
///
/// NB: We assume the lock is held when this call is made
void tmcondvar_signal_lock(tmcondvar_t* cv);

/// Behavior similar to tmcondvar_signal_lock, except that we wake the
/// "newest" thread
///
/// NB: We assume the lock is held when this call is made
void tmcondvar_signal_back_lock(tmcondvar_t* cv);

/// Broadcast code for use when tmcondvars are used from a lock-based context
///
/// NB: We assume the lock is held when this call is made
void tmcondvar_broadcast_lock(tmcondvar_t* cv);

#ifdef __cplusplus
}
#endif
