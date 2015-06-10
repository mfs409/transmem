// (C) Copyright Christian Bienia 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0.
//
//  file : Barrier.cpp
//  author : Christian Bienia - cbienia@cs.princeton.edu
//  description : A barrier

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#if defined(HAVE_LIBPTHREAD)
# include <pthread.h>
# include <errno.h>
#else
// [transmem] use tmcondvars?
# ifdef ENABLE_TM
#  include <tmcondvar.h>
# else
#  include "Mutex.h"
#  include "Condition.h"
# endif
#endif //HAVE_LIBPTHREAD

#include <exception>

#include "Barrier.h"


namespace threads {

Barrier::Barrier(int _n) throw(BarrierException) {
#if defined(HAVE_LIBPTHREAD)
  int rv;

  n = _n;
  rv = pthread_barrier_init(&b, NULL, n);

  switch(rv) {
    case 0:
      break;
    case EINVAL:
    case EBUSY:
    {
      BarrierInitException e;
      throw e;
      break;
    }
    case EAGAIN:
    case ENOMEM:
    {
      BarrierResourceException e;
      throw e;
      break;
    }
    default:
    {
      BarrierUnknownException e;
      throw e;
      break;
    }
  }
#else
  n = _n;
  countSleep = 0;
  countReset = 0;
  // [transmem] init tmcondvars
#ifdef ENABLE_TM
  tmCSleep = tmcondvar_create();
  tmCReset = tmcondvar_create();
#else
  M = new Mutex;
  CSleep = new Condition(*M);
  CReset = new Condition(*M);
#endif
#endif //HAVE_LIBPTHREAD
}

Barrier::~Barrier() throw(BarrierException) {
#if defined(HAVE_LIBPTHREAD)
  int rv;

  rv = pthread_barrier_destroy(&b);

  switch(rv) {
    case 0:
      break;
    case EINVAL:
    case EBUSY:
    {
      BarrierDestroyException e;
      throw e;
      break;
    }
    default:
    {
      BarrierUnknownException e;
      throw e;
      break;
    }
  }
#else
  // [transmem] if we're using TM, let the condvars leak
#ifndef ENABLE_TM
  delete CReset;
  delete CSleep;
  delete M;
#endif
#endif //HAVE_LIBPTHREAD
}

//Wait at a barrier
bool Barrier::Wait() throw(BarrierException) {
#if defined(HAVE_LIBPTHREAD)
  int rv;

  rv = pthread_barrier_wait(&b);

  switch(rv) {
    case 0:
      break;
    case PTHREAD_BARRIER_SERIAL_THREAD:
      return true;
      break;
    case EINVAL:
    {
      BarrierException e;
      throw e;
      break;
    }
    default:
    {
      BarrierUnknownException e;
      throw e;
      break;
    }
  }

  return false;
#else
  bool master;

  // [transmem] custom transactional barrier
#ifdef ENABLE_TM
  // since we need tmcondvar_wait() to always be the last operation in the
  // transaction, we use a 'mode' variable.  0 means "done", and other values
  // correspond to which phase of the complex critical section we're in.
  int mode = 1;
  while (mode) {
    __transaction_atomic {
      if (mode == 1) {
        //Make sure no more than n threads have entered the barrier yet, otherwise wait for reset
        if(countSleep >= n) {
          tmcondvar_wait(tmCReset);
        }
        else {
          mode = 2;
        }
      }

      // the transition from mode == 2 to mode == 4 can be direct, or can be
      // via sleeping and then waking once.  The latter case is captured by
      // mode == 3, a transient state.
      if (mode == 3)
        mode = 4;

      if (mode == 2) {
        //Enter barrier, pick a thread as master
        master = (countSleep == 0);
        countSleep++;

        //Sleep until designated number of threads have entered barrier
        if(countSleep < n) {
          //Wait() must be free of spurious wakeups
          mode = 3;
          tmcondvar_wait(tmCSleep);
        } else {
          countReset = 0;  //prepare for synchronized reset
          tmcondvar_broadcast(tmCSleep);
          mode = 4;
        }
      }

      if (mode == 4) {
        //Leave barrier
        countReset++;

        //Wait until all threads have left barrier, then execute reset
        if(countReset < n) {
          //Wait() must be free of spurious wakeups
          mode = 0;
          tmcondvar_wait(tmCReset);
        } else {
          countSleep = 0;
          tmcondvar_broadcast(tmCReset);
          mode = 0;
        }
      }
    }
  }
#else
  M->Lock();

  //Make sure no more than n threads have entered the barrier yet, otherwise wait for reset
  while(countSleep >= n) CReset->Wait();

  //Enter barrier, pick a thread as master
  master = (countSleep == 0);
  countSleep++;

  //Sleep until designated number of threads have entered barrier
  if(countSleep < n) {
    //Wait() must be free of spurious wakeups
    CSleep->Wait();
  } else {
    countReset = 0;  //prepare for synchronized reset
    CSleep->NotifyAll();
  }

  //Leave barrier
  countReset++;

  //Wait until all threads have left barrier, then execute reset
  if(countReset < n) {
    //Wait() must be free of spurious wakeups
    CReset->Wait();
  } else {
    countSleep = 0;
    CReset->NotifyAll();
  }

  M->Unlock();
#endif

  return master;
#endif //HAVE_LIBPTHREAD
}

const int Barrier::nThreads() const {
  return n;
}

};
