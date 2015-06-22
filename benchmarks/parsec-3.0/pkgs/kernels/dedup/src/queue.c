#include <assert.h>

#include "util.h"
#include "queue.h"
#include "config.h"

#ifdef ENABLE_PTHREADS
#include <pthread.h>
#endif //ENABLE_PTHREADS

void queue_init(queue_t * que, size_t size, int nProducers) {
#ifdef ENABLE_PTHREADS
  // [transmem] initialize condvars
#ifdef ENABLE_TM
  que->tm_notEmpty = tmcondvar_create();
  que->tm_notFull = tmcondvar_create();
#else
  pthread_mutex_init(&que->mutex, NULL);
  pthread_cond_init(&que->notEmpty, NULL);
  pthread_cond_init(&que->notFull, NULL);
#endif
#endif
  assert(!ringbuffer_init(&(que->buf), size));
  que->nProducers = nProducers;
  que->nTerminated = 0;
}

void queue_destroy(queue_t * que) {
#ifdef ENABLE_PTHREADS
  // [transmem] we just leak the condvars at destroy time
#ifndef ENABLE_TM
  pthread_mutex_destroy(&que->mutex);
  pthread_cond_destroy(&que->notEmpty);
  pthread_cond_destroy(&que->notFull);
#endif
#endif
  ringbuffer_destroy(&(que->buf));
}

/* Private function which requires synchronization */
static inline int queue_isTerminated(queue_t * que) {
  // [transmem] Disable asserts
#ifndef ENABLE_TM
  assert(que->nTerminated <= que->nProducers);
#endif
  return que->nTerminated == que->nProducers;
}

void queue_terminate(queue_t * que) {
#ifdef ENABLE_PTHREADS
  // [transmem] TM instead of lock
#ifdef ENABLE_TM
  __transaction_atomic {
#else
  pthread_mutex_lock(&que->mutex);
#endif
#endif
  que->nTerminated++;
  // [transmem] disable asserts
#ifndef ENABLE_TM
  assert(que->nTerminated <= que->nProducers);
#endif
#ifdef ENABLE_PTHREADS
  // [transmem] tmcondvar broadcast, then finish transaction
#ifdef ENABLE_TM
  if(queue_isTerminated(que)) tmcondvar_broadcast(que->tm_notEmpty);
  }
#else
  if(queue_isTerminated(que)) pthread_cond_broadcast(&que->notEmpty);
  pthread_mutex_unlock(&que->mutex);
#endif
#endif
}

int queue_dequeue(queue_t *que, ringbuffer_t *buf, int limit) {
  int i;
  // [transmem] Flatten the ifdefs to make this code easier to understand
#ifdef ENABLE_TM
  while (1) {
  __transaction_atomic {
  if (ringbuffer_isEmpty(&que->buf) && !queue_isTerminated(que)) {
    tmcondvar_wait(que->tm_notEmpty);
    continue; // [transmem] skip rest of critical section & commit transaction
  }
  if (ringbuffer_isEmpty(&que->buf) && queue_isTerminated(que)) {
    return -1;
  }

  //NOTE: This can be optimized by copying whole segments of pointers with memcpy. However,
  //      `limit' is typically small so the performance benefit would be negligible.
  for(i=0; i<limit && !ringbuffer_isEmpty(&que->buf) && !ringbuffer_isFull(buf); i++) {
    void *temp;
    int rv;

    temp = ringbuffer_remove(&que->buf);
    // assert(temp!=NULL); // [transmem] Disable asserts
    rv = ringbuffer_insert(buf, temp);
    // assert(rv==0); // [transmem] disable asserts
  }
  if(i>0) tmcondvar_signal(que->tm_notFull);
  }
  return i;
  } // [transmem] end while loop
#else
#ifdef ENABLE_PTHREADS
  pthread_mutex_lock(&que->mutex);
  while (ringbuffer_isEmpty(&que->buf) && !queue_isTerminated(que)) {
    pthread_cond_wait(&que->notEmpty, &que->mutex);
  }
#endif
  if (ringbuffer_isEmpty(&que->buf) && queue_isTerminated(que)) {
#ifdef ENABLE_PTHREADS
    pthread_mutex_unlock(&que->mutex);
#endif
    return -1;
  }

  //NOTE: This can be optimized by copying whole segments of pointers with memcpy. However,
  //      `limit' is typically small so the performance benefit would be negligible.
  for(i=0; i<limit && !ringbuffer_isEmpty(&que->buf) && !ringbuffer_isFull(buf); i++) {
    void *temp;
    int rv;

    temp = ringbuffer_remove(&que->buf);
    assert(temp!=NULL);
    rv = ringbuffer_insert(buf, temp);
    assert(rv==0);
  }
#ifdef ENABLE_PTHREADS
  if(i>0) pthread_cond_signal(&que->notFull);
  pthread_mutex_unlock(&que->mutex);
#endif
  return i;
#endif
}

int queue_enqueue(queue_t *que, ringbuffer_t *buf, int limit) {
  int i;

  // [transmem] Again, let's pull out the TM code to simplify the ifdefs
#ifdef ENABLE_TM
  while (1) {
  __transaction_atomic {
    // assert(!queue_isTerminated(que)); // [transmem] disable asserts
  if (ringbuffer_isFull(&que->buf)) {
    tmcondvar_wait(que->tm_notFull);
    continue; // [transmem] commit the transaction, wait, loop around
  }

  //NOTE: This can be optimized by copying whole segments of pointers with memcpy. However,
  //      `limit' is typically small so the performance benefit would be negligible.
  for(i=0; i<limit && !ringbuffer_isFull(&que->buf) && !ringbuffer_isEmpty(buf); i++) {
    void *temp;
    int rv;

    temp = ringbuffer_remove(buf);
    // assert(temp!=NULL); // [transmem] disable asserts
    rv = ringbuffer_insert(&que->buf, temp);
    // assert(rv==0); // [transmem] disable asserts
  }
  if(i>0) tmcondvar_signal(que->tm_notEmpty);
  return i;
  } // transaction_atomic
  } // while
#else
#ifdef ENABLE_PTHREADS
  pthread_mutex_lock(&que->mutex);
  assert(!queue_isTerminated(que));
  while (ringbuffer_isFull(&que->buf))
    pthread_cond_wait(&que->notFull, &que->mutex);
#else
  assert(!queue_isTerminated(que));
#endif

  //NOTE: This can be optimized by copying whole segments of pointers with memcpy. However,
  //      `limit' is typically small so the performance benefit would be negligible.
  for(i=0; i<limit && !ringbuffer_isFull(&que->buf) && !ringbuffer_isEmpty(buf); i++) {
    void *temp;
    int rv;

    temp = ringbuffer_remove(buf);
    assert(temp!=NULL);
    rv = ringbuffer_insert(&que->buf, temp);
    assert(rv==0);
  }
#ifdef ENABLE_PTHREADS
  if(i>0) pthread_cond_signal(&que->notEmpty);
  pthread_mutex_unlock(&que->mutex);
#endif
  return i;
#endif
}
