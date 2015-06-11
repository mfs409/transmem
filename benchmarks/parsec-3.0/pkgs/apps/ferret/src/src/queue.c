#include "queue.h"

#include <pthread.h>
#include <stdlib.h>

void queue_init(struct queue * que, int size, int prod_threads) {
  // [transmem] configure condvars and locks according to build config
#ifdef ENABLE_TM
  que->tmEmpty = tmcondvar_create();
  que->tmFull = tmcondvar_create();
#else
  pthread_mutex_init(&que->mutex, NULL);
  pthread_cond_init(&que->empty, NULL);
  pthread_cond_init(&que->full, NULL);
#endif
  que->head = que->tail = 0;
  que->data = (void **)malloc(sizeof(void*) * size);
  que->size = size;
  que->prod_threads = prod_threads;
  que->end_count = 0;
}

void queue_destroy(struct queue* que)
{
  // [transmem] We leak the condvars in TM mode
#ifndef ENABLE_TM
    pthread_mutex_destroy(&que->mutex);
    pthread_cond_destroy(&que->empty);
    pthread_cond_destroy(&que->full);
#endif
    free(que->data);
    que->data = NULL;
}

void queue_signal_terminate(struct queue * que) {
  // [transmem] use transactions
#ifdef ENABLE_TM
  __transaction_atomic {
  que->end_count++;
  tmcondvar_broadcast(que->tmEmpty);
  }
#else
  pthread_mutex_lock(&que->mutex);
  que->end_count++;
  pthread_cond_broadcast(&que->empty);
  pthread_mutex_unlock(&que->mutex);
#endif
}

int dequeue(struct queue * que, void **to_buf) {
  // [transmem] use transactions instead of locks
#ifdef ENABLE_TM
  int mode = 1; // 1 == keep waiting, 2 == will return from this iteration
  while (mode != 0) {
    __transaction_atomic {
      if (mode == 1)
        // chceck if queue is empty?
        if (que->tail == que->head && (que->end_count) < que->prod_threads) {
          tmcondvar_wait(que->tmEmpty);
        }
        else {
          mode = 2;
        }
      if (mode == 2) {
        // check if queue has been terminated?
        if (que->tail == que->head && (que->end_count) == que->prod_threads) {
          tmcondvar_broadcast(que->tmEmpty);
          return -1;
        }

        *to_buf = que->data[que->tail];
        que->tail ++;
        if (que->tail == que->size)
          que->tail = 0;
        tmcondvar_signal(que->tmFull);
        return 0;
      }
    }
  }
#else
    pthread_mutex_lock(&que->mutex);
    // chceck if queue is empty?
    while (que->tail == que->head && (que->end_count) < que->prod_threads) {
        pthread_cond_wait(&que->empty, &que->mutex);
    }
    // check if queue has been terminated?
    if (que->tail == que->head && (que->end_count) == que->prod_threads) {
        pthread_cond_broadcast(&que->empty);
        pthread_mutex_unlock(&que->mutex);
        return -1;
    }

    *to_buf = que->data[que->tail];
    que->tail ++;
    if (que->tail == que->size)
  que->tail = 0;
    pthread_cond_signal(&que->full);
    pthread_mutex_unlock(&que->mutex);
    return 0;
#endif
}

void enqueue(struct queue * que, void *from_buf) {
  // [transmem] use transactions instead of locks
#ifdef ENABLE_TM
  // NB: Here's an example of simpler control flow for the condvar
  //     transformation
  while (1) {
    __transaction_atomic {
      if (que->head == (que->tail-1+que->size)%que->size) {
        tmcondvar_wait(que->tmFull);
      }
      else {
        que->data[que->head] = from_buf;
        que->head ++;
        if (que->head == que->size)
          que->head = 0;
        tmcondvar_signal(que->tmEmpty);
        return;
      }
    }
  }
#else
    pthread_mutex_lock(&que->mutex);
    while (que->head == (que->tail-1+que->size)%que->size)
  pthread_cond_wait(&que->full, &que->mutex);

    que->data[que->head] = from_buf;
    que->head ++;
    if (que->head == que->size)
  que->head = 0;

    pthread_cond_signal(&que->empty);
    pthread_mutex_unlock(&que->mutex);
#endif
}
