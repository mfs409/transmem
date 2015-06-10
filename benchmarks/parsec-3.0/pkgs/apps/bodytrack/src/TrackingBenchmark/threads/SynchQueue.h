// (C) Copyright Christian Bienia 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0.
//
//  file : SynchQueue.h
//  author : Christian Bienia - cbienia@cs.princeton.edu
//  description : A synchronized queue

#ifndef SYNCHQUEUE_H
#define SYNCHQUEUE_H

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <pthread.h>
#include <queue>

// [transmem] use tmcondvars instead of locks and condvars
#ifdef ENABLE_TM
#include <tmcondvar.h>
#else
#include "Mutex.h"
#include "Condition.h"
#endif


namespace threads {

//General queue exception
class SynchQueueException: public std::exception {
  public:
    const char *what() {return "Unspecified synchronization queue error";}
};

//capacity constant for queues with no maximum capacity
#define SYNCHQUEUE_NOCAPACITY -1

template <typename T>
class SynchQueue {
  public:
    //In addition to the default (unbounded) queue behavior, synchronized queues support a maximum capacity
    SynchQueue();
    SynchQueue(int);
    ~SynchQueue();

    bool isEmpty() const;
    bool isFull() const;
    int Size() const;
    const int Capacity() const;
    void Enqueue(const T&);
    const T &Dequeue();

  private:
    std::queue<T> q;
    int cap;
    // [transmem] use tmcondvars instead of locks/condvars
#ifdef ENABLE_TM
    tmcondvar_t* tmNotEmpty;
    tmcondvar_t* tmNotFull;
#else
    Mutex *M;
    Condition *notEmpty;
    Condition *notFull;
#endif
};

template <typename T>
SynchQueue<T>::SynchQueue() {
  cap = SYNCHQUEUE_NOCAPACITY;
  // [transmem] init tmcondvars
#ifdef ENABLE_TM
  tmNotEmpty = tmcondvar_create();
  tmNotFull = tmcondvar_create();
#else
  M = new Mutex;
  notEmpty = new Condition(M);
  notFull = new Condition(M);
#endif
}

template <typename T>
SynchQueue<T>::SynchQueue(int _cap) {
  if(_cap < 1) {
    SynchQueueException e;
    throw e;
  }

  cap = _cap;
  // [transmem] init tmcondvars
#ifdef ENABLE_TM
  tmNotEmpty = tmcondvar_create();
  tmNotFull = tmcondvar_create();
#else
  M = new Mutex;
  notEmpty = new Condition(*M);
  notFull = new Condition(*M);
#endif
}

template <typename T>
SynchQueue<T>::~SynchQueue() {
  // [transmem] in TM mode, we just let the condvars leak
#ifndef ENABLE_TM
  delete notFull;
  delete notEmpty;
  delete M;
#endif
}

template <typename T>
bool SynchQueue<T>::isEmpty() const {
  bool rv;

  // [transmem] use tm instead of locks
#ifdef ENABLE_TM
  __transaction_atomic {
    rv = q.empty();
  }
#else
  M->Lock();
  rv = q.empty();
  M->Unlock();
#endif

  return rv;
}

template <typename T>
bool SynchQueue<T>::isFull() const {
  int s;

  if(cap == SYNCHQUEUE_NOCAPACITY) {
    return false;
  }

  // [transmem] use tm instead of locks
#ifdef ENABLE_TM
  __transaction_atomic {
    s = q.size();
  }
#else
  M->Lock();
  s = q.size();
  M->Unlock();
#endif

  return (cap == s);
}

template <typename T>
int SynchQueue<T>::Size() const {
  int s;

  // [transmem] use tm instead of locks
#ifdef ENABLE_TM
  __transaction_atomic {
    s = q.size();
  }
#else
  M->Lock();
  s = q.size();
  M->Unlock();
#endif

  return s;
}

template <typename T>
const int SynchQueue<T>::Capacity() const {
  return cap;
}

template <typename T>
void SynchQueue<T>::Enqueue(const T &x) {
  // [transmem] use tm instead of locks... need to change control flow so
  //            that wait() is last op in transaction
#ifdef ENABLE_TM
  bool done = false;
  while (!done) {
    __transaction_atomic {
      if(q.size() >= cap && cap != SYNCHQUEUE_NOCAPACITY) {
        tmcondvar_wait(tmNotFull);
      }
      else {
        q.push(x);
        tmcondvar_signal(tmNotEmpty);
        done = true;
      }
    }
  }
#else
  M->Lock();
  while(q.size() >= cap && cap != SYNCHQUEUE_NOCAPACITY) {
    notFull->Wait();
  }
  q.push(x);
  notEmpty->NotifyOne();
  M->Unlock();
#endif
}

template <typename T>
const T &SynchQueue<T>::Dequeue() {
  // [transmem] use tm instead of locks... need to change control flow so
  //            that wait() is last op in transaction
#ifdef ENABLE_TM
  bool done = false;
  while (!done) {
    __transaction_atomic {
      if (q.empty()) {
        tmcondvar_wait(tmNotEmpty);
      }
      else {
        T &x = q.front();
        q.pop();
        tmcondvar_signal(tmNotFull);
        done = true;
      }
    }
  }
#else
  M->Lock();
  while(q.empty()) {
    notEmpty->Wait();
  }
  T &x = q.front();
  q.pop();
  notFull->NotifyOne();
  M->Unlock();
#endif

  return x;
}

} //namespace threads

#endif //SYNCHQUEUE_H
