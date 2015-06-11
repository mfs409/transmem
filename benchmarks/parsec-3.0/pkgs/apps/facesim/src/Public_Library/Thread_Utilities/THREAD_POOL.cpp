//#####################################################################
// Copyright 2004, Andrew Selle.
// This file is part of PhysBAM whose distribution is governed by the license contained in the accompanying file PHYSBAM_COPYRIGHT.txt.
//#####################################################################
#include <iostream>
#include "THREAD_POOL.h"
#ifndef ENABLE_TM
#include "THREAD_CONDITION.h"
#include "THREAD_LOCK.h"
#endif

#ifdef ENABLE_PTHREADS
#include <pthread.h>
// [transmem] require tmcondvar...
# ifdef ENABLE_TM
#  include <tmcondvar.h>
# endif
#endif //ENABLE_PTHREADS

#include "../Data_Structures/QUEUE.h"
#include "../Data_Structures/PAIR.h"
using namespace PhysBAM;

#ifndef USE_ALAMERE_TASKQ

void *Worker (void *);
void Exit_Thread (long, void*);

class THREAD_POOL_SINGLETON_DATA
{
public:
    // [transmem] in TM mode, no locks, and use tmcondvars
#ifdef ENABLE_TM
    tmcondvar_t* tm_workers_done_condition;
    tmcondvar_t* tm_new_work_condition;
#else
    THREAD_CONDITION workers_done_condition, new_work_condition;
    THREAD_LOCK lock;
#endif
#ifdef ENABLE_PTHREADS
    ARRAY<pthread_t> threads;
#endif //ENABLE_PTHREADS
    QUEUE<PAIR<THREAD_POOL::CALLBACK, void*> > queue;
    int working_threads, waiting_threads;

    THREAD_POOL_SINGLETON_DATA()
        : queue (65536)
    {
    }
};
static THREAD_POOL_SINGLETON_DATA* data = 0;

THREAD_POOL* THREAD_POOL::singleton_instance = 0;

//#####################################################################
// Function Worker
//#####################################################################
void*
Worker (void* pointer)
{
    THREAD_POOL& pool = * (THREAD_POOL*) pointer;
#ifdef ENABLE_PTHREADS
    int thread_id = pthread_self();
#else
    int 0;
#endif //ENABLE_PTHREADS

    // [transmem] initialize thread's tmcondvar context
#ifdef ENABLE_TM
    tmcondvar_init_thread();
#endif

    while (1)
    {
      // [transmem] use TM instead of locks
#ifdef ENABLE_TM
      // mode == 0 means we finished
      // mode == 1 means we're going to check if the queue is empty
      // mode == 2 means that we are waking from seeing the queue empty
      // mode == 3 means we didn't see an empty queue
      int mode = 1;
      while (mode != 0) {
        __transaction_atomic {
          if (mode == 2) {
            data->working_threads++;
            data->waiting_threads--;
            mode = 1;
          }

          if (mode == 1) {
            if (data->queue.Empty()) {
              data->working_threads--;
              if (data->working_threads == 0) tmcondvar_signal(data->tm_workers_done_condition);
              data->waiting_threads++;
              tmcondvar_wait(data->tm_new_work_condition);
              mode = 2;
            }
            else {
              mode = 3;
            }
          }

          if (mode == 3) {
            // remove task and do it
            PAIR<THREAD_POOL::CALLBACK, void*> work = data->queue.Dequeue();
            mode = 0;
          }
        }
      }
#else
        data->lock.Lock();

        while (data->queue.Empty())
        {
            data->working_threads--;

            if (data->working_threads == 0) data->workers_done_condition.Signal();

            data->waiting_threads++;
            data->new_work_condition.Wait (data->lock);
            data->working_threads++;
            data->waiting_threads--;
        }

        // remove task and do it
        PAIR<THREAD_POOL::CALLBACK, void*> work = data->queue.Dequeue();
        data->lock.Unlock();
#endif
        work.x (thread_id, work.y);
    }
}
//#####################################################################
// Function THREAD_POOL
//#####################################################################
THREAD_POOL::
THREAD_POOL()
{
    std::cout << "THREAD_POOL: Initializing condition variables" << std::endl;
    char* threads_environment = getenv ("PHYSBAM_THREADS");

    if (threads_environment) number_of_threads = atoi (threads_environment);
    else number_of_threads = 2;

#ifdef ALAMERE_PTHREADS
    std::cout << "Initializing Alamere Threads" << std::endl;
    ALAMERE_INIT (number_of_threads + 1);
#endif
    data = new THREAD_POOL_SINGLETON_DATA();
    data->threads.Resize_Array (number_of_threads);
    data->working_threads = number_of_threads;
    data->waiting_threads = 0;
    std::cout << "THREAD_POOL: Inititalizing Threads: ";

    for (int t = 1; t <= number_of_threads; t++)
    {
        std::cout << t << " ";
#ifdef ENABLE_PTHREADS
        pthread_create (&data->threads (t), 0, Worker, this);
    }

#else
        assert (0);
#endif //ENABLE_PTHREADS
    std::cout << std::endl;
}
//#####################################################################
// Function ~THREAD_POOL
//#####################################################################
THREAD_POOL::
~THREAD_POOL()
{
    for (int t = 1; t <= number_of_threads; t++) Add_Task (Exit_Thread, 0);

#ifdef ENABLE_PTHREADS

    for (int t = 1; t <= number_of_threads; t++) pthread_join (data->threads (t), 0);

#endif //ENABLE_PTHREADS
    delete data;
#ifdef ALAMERE_PTHREADS
    ALAMERE_END();
#endif
}
//#####################################################################
// Function Add_Task
//#####################################################################
void THREAD_POOL::
Add_Task (CALLBACK callback_input, void* data_input)
{
    // [transmem] use TM instead of locks
#ifdef ENABLE_TM
    __transaction_atomic {
        if (data->waiting_threads) tmcondvar_signal(data->tm_new_work_condition);
        data->queue.Enqueue (PAIR<CALLBACK, void*> (callback_input, data_input));
    }
#else
    data->lock.Lock();

    if (data->waiting_threads) data->new_work_condition.Signal();

    data->queue.Enqueue (PAIR<CALLBACK, void*> (callback_input, data_input));
    data->lock.Unlock();
#endif
}
//#####################################################################
// Function Add_TaskGrid
//#####################################################################
void THREAD_POOL::
Add_Task (CALLBACK callback_input, int numTasks)
{
    // not supported
    exit (-1);
}
//#####################################################################
// Function Wait_For_Completion
//#####################################################################
void THREAD_POOL::
Wait_For_Completion()
{
    // [transmem] use tmcondvars...
#ifdef ENABLE_TM
    bool done = false;
    while (!done) {
        __transaction_atomic {
            if (!data->queue.Empty() || data->working_threads != 0)
                tmcondvar_wait(data->tm_workers_done_condition);
            else
                done = true;
        }
    }
#else
    data->lock.Lock();

    while (!data->queue.Empty() || data->working_threads != 0) data->workers_done_condition.Wait (data->lock);

    data->lock.Unlock();
#endif
}
//#####################################################################
// Function Exit_Thread
//#####################################################################
void Exit_Thread (long thread_id, void* data)
{
#ifdef ENABLE_PTHREADS
    pthread_exit (0);
#endif //ENABLE_PTHREADS
}
//#####################################################################
#endif
