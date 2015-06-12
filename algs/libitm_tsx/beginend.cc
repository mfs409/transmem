/* Copyright (C) 2008-2015 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@redhat.com>.

   This file is part of the GNU Transactional Memory Library (libitm).

   Libitm is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   Libitm is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include "libitm_i.h"
#include <pthread.h>


using namespace GTM;

#if !defined(HAVE_ARCH_GTM_THREAD) || !defined(HAVE_ARCH_GTM_THREAD_DISP)
extern __thread gtm_thread_tls _gtm_thr_tls;
#endif

// [transmem] removed serial_lock
gtm_thread *GTM::gtm_thread::list_of_threads = 0;
unsigned GTM::gtm_thread::number_of_threads = 0;

// [transmem] removed stmlock array, gtm_clock, and gtm_spin_count_var

#ifdef HAVE_64BIT_SYNC_BUILTINS
static atomic<_ITM_transactionId_t> global_tid;
#else
static _ITM_transactionId_t global_tid;
static pthread_mutex_t global_tid_lock = PTHREAD_MUTEX_INITIALIZER;
#endif


// Provides a on-thread-exit callback used to release per-thread data.
static pthread_key_t thr_release_key;
static pthread_once_t thr_release_once = PTHREAD_ONCE_INIT;

// See gtm_thread::begin_transaction.
uint32_t GTM::htm_fastpath = 0;

/* Allocate a transaction structure.  */
void *
GTM::gtm_thread::operator new (size_t s)
{
  void *tx;

  assert(s == sizeof(gtm_thread));

  tx = xmalloc (sizeof (gtm_thread), true);
  memset (tx, 0, sizeof (gtm_thread));

  return tx;
}

/* Free the given transaction. Raises an error if the transaction is still
   in use.  */
void
GTM::gtm_thread::operator delete(void *tx)
{
  free(tx);
}

static void
thread_exit_handler(void *)
{
  gtm_thread *thr = gtm_thr();
  if (thr)
    delete thr;
  set_gtm_thr(0);
}

static void
thread_exit_init()
{
  if (pthread_key_create(&thr_release_key, thread_exit_handler))
    GTM_fatal("Creating thread release TLS key failed.");
}

// [transmem] Add a simple spinlock for coordinating hardware and software
//            transactions
namespace {
  static std::atomic<bool> tm_lock(0);
}

void spinlock_acquire() {
    do {
        while (tm_lock.load()) { }
    } while (tm_lock.exchange(true));
}

void spinlock_release() {
    tm_lock.store(false, std::memory_order_release);
}

bool spinlock_held() {
    return tm_lock.load();
}

bool spinlock_held_relaxed() {
    return tm_lock.load(std::memory_order_relaxed);
}



GTM::gtm_thread::~gtm_thread()
{
  if (nesting > 0)
    GTM_fatal("Thread exit while a transaction is still active.");

  // Deregister this transaction.
  // [transmem] use spinlock instead of serial_lock
  spinlock_acquire();
  gtm_thread **prev = &list_of_threads;
  for (; *prev; prev = &(*prev)->next_thread)
    {
      if (*prev == this)
    {
      *prev = (*prev)->next_thread;
      break;
    }
    }
  number_of_threads--;
  number_of_threads_changed(number_of_threads + 1, number_of_threads);
  spinlock_release();
}

GTM::gtm_thread::gtm_thread ()
{
  // This object's memory has been set to zero by operator new, so no need
  // to initialize any of the other primitive-type members that do not have
  // constructors.
    //
    // [transmem] we probably don't need shared_state anymore
  shared_state.store(-1, memory_order_relaxed);

  // Register this transaction with the list of all threads' transactions.
  // [transmem] use spinlock instead of serial_lock
  spinlock_acquire();
  next_thread = list_of_threads;
  list_of_threads = this;
  number_of_threads++;
  number_of_threads_changed(number_of_threads - 1, number_of_threads);
  spinlock_release();

  if (pthread_once(&thr_release_once, thread_exit_init))
    GTM_fatal("Initializing thread release TLS key failed.");
  // Any non-null value is sufficient to trigger destruction of this
  // transaction when the current thread terminates.
  if (pthread_setspecific(thr_release_key, this))
    GTM_fatal("Setting thread release TLS key failed.");
}

// [transmem] We no longer need choose_code_path... we always use the
//            uninstrumented path

// [transmem] The entire machinery for _ITM_beginTransaction changes in this
//            implementation.  We don't require inline assembly, instead we
//            can use the implementation technique proposed in
//              https://software.intel.com/en-us/blogs/2012/11/06/
//              exploring-intel-transactional-synchronization-extensions-with-intel-software
//            which lets us use _xbegin() and _xend() directly in a C
//            function, instead of having _ITM_beginTransaction as an ASM
//            function that calls to gtm_thread::begin_transaction

uint32_t
_ITM_beginTransaction (uint32_t prop, ...)
{
  // [transmem] One of the main benefits of this new approach is that we can
  //            do easy initialization of the gtm thread context right here.
  //            That, in turn, means that we can be guaranteed to have a
  //            thread context, which in turn means we can use onCommit
  //            handlers even in HTM
  gtm_thread *tx = gtm_thr();
  if (unlikely(tx == NULL)) {
    tx = new gtm_thread();
    set_gtm_thr(tx);
  }

  // [transmem] Since we care about onCommit handlers, we're not going to use
  //            the HTM's built-in nesting support (which is also limited in
  //            depth), but instead manually track it via the nesting field
  //            of the guaranteed-to-exist descriptor, and then use the start
  //            routine from the above blog post
  if (likely(tx->nesting == 0)) {
    int attempts = 0;
    while (true) {
      // try to start a transaction, make sure lock unheld after tx begins
      ++attempts;
      uint32_t status = _xbegin();
      if (status == _XBEGIN_STARTED) {
        if (!spinlock_held_relaxed())
          break;
        _xabort(0xFF);
      }
      // either tx failed, or lock held when tx attempted
      else {
        // couldn't start because lock held, so wait:
        if ((status & _XABORT_EXPLICIT) && (_XABORT_CODE(status) == 0xFF))
          while (spinlock_held()) { }
        // go serial on hard abort or >5 tries
        if ((attempts > 5) || (!(status & _XABORT_RETRY))) {
          spinlock_acquire();
          break;
        }
      }
    }
  }

  // [transmem] increment the nesting depth and save properties
  ++tx->nesting;
  tx->prop = prop;

  // [transmem] we may not need transaction IDs anymore?
  static const _ITM_transactionId_t tid_block_size = 1 << 16;
  // As long as we have not exhausted a previously allocated block of TIDs,
  // we can avoid an atomic operation on a shared cacheline.
  if (tx->local_tid & (tid_block_size - 1))
    tx->id = tx->local_tid++;
  else
    {
#ifdef HAVE_64BIT_SYNC_BUILTINS
      // We don't really care which block of TIDs we get but only that we
      // acquire one atomically; therefore, relaxed memory order is
      // sufficient.
      tx->id = global_tid.fetch_add(tid_block_size, memory_order_relaxed);
      tx->local_tid = tx->id + 1;
#else
      pthread_mutex_lock (&global_tid_lock);
      global_tid += tid_block_size;
      tx->id = global_tid;
      tx->local_tid = tx->id + 1;
      pthread_mutex_unlock (&global_tid_lock);
#endif
    }

  // [transmem] We always run uninstrumented code, because we're either HTM
  //            or IRR
  return a_runUninstrumentedCode;
}

// [transmem] We no longer have gtm_transaction_cp methods to implement

// [transmem] Rollback is via HTM, so no method call needed anymore

// [transmem] We do not support transaction cancel, so abortTransaction
//            should crash the program
void ITM_REGPARM
_ITM_abortTransaction (_ITM_abortReason reason)
{
  abort();
}

// [transmem] There is no trycommit anymore... we go straight to a
//            streamlined _ITM_commitTransaction

void ITM_REGPARM
_ITM_commitTransaction(void)
{
  // [transmem] use nesting to decide if we are *really* committing
  gtm_thread *tx = gtm_thr();
  -- tx->nesting;

  if (!tx->nesting) {
    // [transmem] Either we're in HTM or we have the serial lock.  Either
    //            way, committing is easy
    if (likely(!spinlock_held_relaxed()))
      _xend();
    else
      spinlock_release();

    // [transmem] since we've got a descriptor, we can do user actions!
    tx->commit_user_actions();
    tx->commit_allocations(false, 0);
  }
}

// [transmem] In this STM library, exception support is really limited, so we
//            just commit on exception.
void ITM_REGPARM
_ITM_commitTransactionEH(void *exc_ptr)
{
  _ITM_commitTransaction();
}

/// [transmem] Once we clean up the dispatch code a little more, we can get
//             rid of this function
void
GTM::gtm_thread::restart(gtm_restart_reason r, bool finish_serial_upgrade) {
    abort();
}
