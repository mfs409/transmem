/* Copyright (C) 2012-2015 Free Software Foundation, Inc.
   Contributed by Torvald Riegel <triegel@redhat.com>.

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

using namespace GTM;

namespace {

// This group consists of all TM methods that detect conflicts via
// value-based validation
struct norec_mg : public method_group
{
  // Maximum time is all bits except the lock bit and the overflow reserve bit
  static const gtm_word TIME_MAX = (~(gtm_word)0 >> 2);

  // The shared time base.
  atomic<gtm_word> time __attribute__((aligned(HW_CACHELINE_SIZE)));

  virtual void init()
  {
    // This store is only executed while holding the serial lock, so relaxed
    // memory order is sufficient here.
    time.store(0, memory_order_relaxed);
  }

  virtual void fini() { }

  // We only re-initialize when our time base overflows.  Thus, only reset
  // the time base and the orecs but do not re-allocate the orec array.
  virtual void reinit()
  {
    // This store is only executed while holding the serial lock, so relaxed
    // memory order is sufficient here.
    time.store(0, memory_order_relaxed);
  }
};

static norec_mg o_norec_mg;

// The NOrec TM method.
// Uses a single global lock to protect writeback, and uses values for
// validation
//
// Note that NOrec does not require quiescence-based validation, though we
// employ it to simplify memory reclamation and remain consistent with the
// rest of the GCC-TM implementations
class norec_dispatch : public abi_dispatch
{
protected:

  // Returns true iff all locations read by the transaction still have the
  // values observed by the transaction
  static gtm_word validate(gtm_thread *tx)
  {
    while (true) {
      // read the lock until it is even
      gtm_word s = o_norec_mg.time.load(memory_order_acquire);
      if ((s & 1) == 1)
        continue;

      // check the read set... the read set is technically an "undo log", but
      // that's not important to this code
      bool valid = tx->valuelog.valuecheck();

      if (!valid)
          // does not allow -1, gtm_word is unsigned int
          return -1;

      // make sure lock didn't change during validation
      tx->shared_state.store(s, memory_order_release);
      if (o_norec_mg.time.load(memory_order_acquire) == s)
          return s;
    }
  }

  template <typename V> static V load(const V* addr, ls_modifier mod)
  {
      gtm_thread *tx = gtm_thr();

      // stack filter: if we are reading from a stack location, just read it
      // without logging or write set lookup
      void *top = mask_stack_top(tx);
      void *bot = mask_stack_bottom(tx);
      if ((addr <= top && (uint8_t*) addr > bot ) ||
          ((uint8_t*)addr < bot && ((uint8_t*)addr + sizeof(V) > bot))) {
          return *addr;
      }

      // check the redo log
      V v;
      if (!tx->redolog_bst.isEmpty() && tx->redolog_bst.find(addr, v) != 0)
        return v;

      // NOrec read loop:
      // A read is valid iff it occurs during a period where the seqlock does
      // not change and is even.  This code also polls for new changes that
      // might necessitate a validation.
      v = *addr;
      // get start time, compared to the current timestamp
      gtm_word start_time = tx->shared_state.load(memory_order_acquire);
      while (start_time != o_norec_mg.time.load(memory_order_acquire)) {
          if ((start_time = validate(tx)) == (gtm_word)-1) {
              tx->restart_reason[RESTART_VALIDATE_READ]++;
              tx->restart(RESTART_VALIDATE_READ);
          }//Abort

          v = *addr;
      }

      // log address and value into read log
      tx->valuelog.log_read(addr, sizeof(V), &v);
      return v;
  }

  template <typename V> static void store(V* addr, const V value,
      ls_modifier mod)
  {
    // filter out writes to the stack frame
    gtm_thread *tx = gtm_thr();
    void *top = mask_stack_top(tx);
    void *bot = mask_stack_bottom(tx);
    if ((addr <= top && (uint8_t*) addr > bot ) ||
        ((uint8_t*)addr < bot && ((uint8_t*)addr + sizeof(V) > bot))) {
      *addr = value;
      return;
    }

    // insert into the log, so we can write it back later
    tx->redolog_bst.insert(addr, value);
  }

public:
  static void memtransfer_static(void *dst, const void* src, size_t size,
      bool may_overlap, ls_modifier dst_mod, ls_modifier src_mod)
  {
    // [transmem] This code is far from optimal

    // [transmem] we copy byte-by-byte, since we need to do log lookups.
    // That means we can treat the type as unsigned chars.
    //
    // Note that we can ignore overlap, since we do buffered writes
    unsigned char *srcaddr = (unsigned char*)const_cast<void*>(src);
    unsigned char *dstaddr = (unsigned char*)dst;

    for (size_t i = 0; i < size; i++) {
      unsigned char temp = load<unsigned char>(srcaddr, RaR);
      store<unsigned char>(dstaddr, temp, WaW);
      dstaddr = (unsigned char*) ((long long)dstaddr + sizeof(unsigned char));
      srcaddr = (unsigned char*) ((long long)srcaddr + sizeof(unsigned char));
    }
  }

  static void memset_static(void *dst, int c, size_t size, ls_modifier mod)
  {
    unsigned char* dstaddr = (unsigned char*)dst;

    // [transmem] save data into redo log... note that the modifier doesn't
    //            matter
    for (size_t it = 0; it < size; it++) {
      store<unsigned char>(dstaddr, (unsigned char)c, WaW);
      dstaddr = (unsigned char*) ((long long)dst + sizeof(unsigned char));
    }
  }

  virtual gtm_restart_reason begin_or_restart()
  {
    // NB: We don't need to do anything for nested transactions.
    gtm_thread *tx = gtm_thr();

    // Read the current time, which becomes our snapshot time.
    // Use acquire memory oder so that we see the lock acquisitions by update
    // transcations that incremented the global time (see trycommit()).
    gtm_word snapshot = o_norec_mg.time.load(memory_order_acquire);
    // Sample the sequence lock, if it is even decrement by 1
    snapshot = snapshot & ~(1L);

    // Re-initialize method group on time overflow.
    if (snapshot >= o_norec_mg.TIME_MAX)
      return RESTART_INIT_METHOD_GROUP;

    // We don't need to enforce any ordering for the following store. There
    // are no earlier data loads in this transaction, so the store cannot
    // become visible before those (which could lead to the violation of
    // privatization safety). The store can become visible after later loads
    // but this does not matter because the previous value will have been
    // smaller or equal (the serial lock will set shared_state to zero when
    // marking the transaction as active, and restarts enforce immediate
    // visibility of a smaller or equal value with a barrier (see
    // rollback()).
    tx->shared_state.store(snapshot, memory_order_relaxed);
    return NO_RESTART;
  }

  virtual bool trycommit(gtm_word& priv_time)
  {
    gtm_thread* tx = gtm_thr();
    gtm_word start_time = 0;

    // If we haven't updated anything, we can commit. Just clean value log.
    if (tx->redolog_bst.isEmpty()) {
      tx->valuelog.commit();
      return true;
    }

    // get start time
    start_time = tx->shared_state.load(memory_order_relaxed);

    // get the lock and validate
    // compare_exchange_weak should save some overhead in a loop?
    while (!o_norec_mg.time.compare_exchange_weak
           (start_time, start_time + 1, memory_order_acquire)) {
      if ((start_time = validate(tx)) == (gtm_word)-1) {
        tx->restart_reason[RESTART_VALIDATE_READ]++;
        return false;
      }
    }

    // do write back
    tx->redolog_bst.writeback();

    // relaese the sequence lock
    gtm_word ct = start_time + 2;
    o_norec_mg.time.store(ct, memory_order_release);

    // We're done, clear the logs.
    tx->redolog_bst.reset();
    // NB: this clears the log, although it is called "commit"
    tx->valuelog.commit();

    // Need to ensure privatization safety. Every other transaction must
    // have a snapshot time that is at least as high as our commit time
    // (i.e., our commit must be visible to them).
    priv_time = ct;
    return true;
  }

  virtual void rollback()
  {
    gtm_thread *tx = gtm_thr();

    // We need this release fence to ensure that privatizers see the
    // rolled-back original state (not any uncommitted values) when they read
    // the new snapshot time that we write in begin_or_restart().
    atomic_thread_fence(memory_order_release);

    // We're done, clear the logs.
    tx->redolog_bst.reset();
    // NB: this clears the log, although it is called "commit"
    tx->valuelog.commit();
  }

  virtual bool supports(unsigned number_of_threads)
  {
    // NOrec can support any number of threads
    return true;
  }

  CREATE_DISPATCH_METHODS(virtual, )
  CREATE_DISPATCH_METHODS_MEM()

  norec_dispatch() : abi_dispatch(false, true, false, 0, &o_norec_mg)
  { }
};

} // anon namespace

static const norec_dispatch o_norec_dispatch;

abi_dispatch *
GTM::dispatch_norec ()
{
  return const_cast<norec_dispatch *>(&o_norec_dispatch);
}
