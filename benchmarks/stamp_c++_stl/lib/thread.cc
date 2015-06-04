/* =============================================================================
 *
 * thread.c
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 *
 * ------------------------------------------------------------------------
 *
 * For the license of ssca2, please see ssca2/COPYRIGHT
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 *
 * ------------------------------------------------------------------------
 *
 * Unless otherwise noted, the following license applies to STAMP files:
 *
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */


#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include "thread.h"

static __thread long      global_threadId;
static long               global_numThread         = 1;
static pthread_barrier_t* global_barrierPtr        = NULL;
static long*              global_threadIds         = NULL;
static pthread_t*         global_threads           = NULL;
static void               (*global_funcPtr)(void*) = NULL;
static void*              global_argPtr            = NULL;
static volatile bool      global_doShutdown        = false;

/**
 * threadWait: Synchronizes all threads to start/stop parallel section
 */
static void* threadWait(void* argPtr)
{
    long threadId = *(long*)argPtr;

    global_threadId = (long)threadId;

    while (1) {
        pthread_barrier_wait(global_barrierPtr); /* wait for start parallel */
        if (global_doShutdown) {
            break;
        }
        global_funcPtr(global_argPtr);
        pthread_barrier_wait(global_barrierPtr); /* wait for end parallel */
        if (threadId == 0) {
            break;
        }
    }
    return NULL;
}

/**
 * thread_startup: Create pool of secondary threads.  numThread is total
 *                 number of threads (primary + secondaries)
 */
void thread_startup(long numThread)
{
    global_numThread = numThread;
    global_doShutdown = false;

    // Set up barrier
    assert(global_barrierPtr == NULL);
    global_barrierPtr = (pthread_barrier_t*)malloc(sizeof(pthread_barrier_t));
    assert(global_barrierPtr);
    pthread_barrier_init(global_barrierPtr, 0, numThread);

    // Set up ids
    assert(global_threadIds == NULL);
    global_threadIds = (long*)malloc(numThread * sizeof(long));
    assert(global_threadIds);
    for (long i = 0; i < numThread; i++) {
        global_threadIds[i] = i;
    }

    // Set up thread list
    assert(global_threads == NULL);
    global_threads = (pthread_t*)malloc(numThread * sizeof(pthread_t));
    assert(global_threads);

    // Set up pool
    for (long i = 1; i < numThread; i++) {
        pthread_create(&global_threads[i], NULL, &threadWait, (void*)&global_threadIds[i]);
    }

    // Wait for primary thread to call thread_start
}

/**
 * thread_start: Make primary and secondary threads execute work.  Should
 *               only be called by primary thread.  funcPtr takes one
 *               arguments: argPtr.
 */
void thread_start(void (*funcPtr)(void*), void* argPtr)
{
    global_funcPtr = funcPtr;
    global_argPtr = argPtr;

    long threadId = 0; /* primary */
    threadWait((void*)&threadId);
}

/**
 * thread_shutdown: Primary thread kills pool of secondary threads
 */
void thread_shutdown()
{
    // Make secondary threads exit wait()
    global_doShutdown = true;
    pthread_barrier_wait(global_barrierPtr);

    long numThread = global_numThread;

    for (long i = 1; i < numThread; i++) {
        pthread_join(global_threads[i], NULL);
    }

    free(global_barrierPtr);
    global_barrierPtr = NULL;

    free(global_threadIds);
    global_threadIds = NULL;

    free(global_threads);
    global_threads = NULL;

    global_numThread = 1;
}

/**
 * thread_getId: Call after thread_start() to get thread ID inside parallel
 *               region
 */
long thread_getId()
{
    return global_threadId;
}

/**
 * thread_getNumThread: Call after thread_start() to get number of threads
 *                      inside parallel region
 */
long thread_getNumThread()
{
    return global_numThread;
}

/**
 * thread_barrier_wait: Call after thread_start() to synchronize threads
 *                      inside parallel region
 */
void thread_barrier_wait()
{
    pthread_barrier_wait(global_barrierPtr);
}

/* =============================================================================
 * TEST_THREAD
 * =============================================================================
 */
#ifdef TEST_THREAD

#include <stdio.h>
#include <unistd.h>

#define NUM_THREADS    (4)
#define NUM_ITERATIONS (3)

void printId (void* argPtr)
{
    long threadId = thread_getId();
    long numThread = thread_getNumThread();
    long i;

    for ( i = 0; i < NUM_ITERATIONS; i++ ) {
        thread_barrier_wait();
        if (threadId == 0) {
            sleep(1);
        } else if (threadId == numThread-1) {
            usleep(100);
        }
        printf("i = %li, tid = %li\n", i, threadId);
        if (threadId == 0) {
            puts("");
        }
        fflush(stdout);
    }
}

int main ()
{
    puts("Starting...");

    /* Run in parallel */
    thread_startup(NUM_THREADS);
    /* Start timing here */
    thread_start(printId, NULL);
    thread_start(printId, NULL);
    thread_start(printId, NULL);
    /* Stop timing here */
    thread_shutdown();

    puts("Done.");

    return 0;
}

#endif /* TEST_THREAD */
