#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

// [transmem] Include tmcondvar support
#ifdef ENABLE_TM
# include <tmcondvar.h>
#endif

struct queue {
  int head, tail;
  void ** data;
  int size;
  int prod_threads;   // no of producing threads
  int end_count;
  // [transmem] use tmcondvars instead of locks and condvars
#ifdef ENABLE_TM
  tmcondvar_t* tmEmpty;
  tmcondvar_t* tmFull;
#else
  pthread_mutex_t mutex;
  pthread_cond_t empty, full;
#endif
};

void queue_signal_terminate(struct queue * que);
void queue_init(struct queue* que, int size, int prod_threads);
void queue_destroy(struct queue* que);
int  dequeue(struct queue* que, void** to_buf);
void enqueue(struct queue* que, void* from_buf);

#endif //QUEUE
