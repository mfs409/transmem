// -*-c++-*-
//
//  Copyright (C) 2011, 2014
//  University of Rochester Department of Computer Science
//    and
//  Lehigh University Department of Computer Science and Engineering
//
// License: Modified BSD
//          Please see the file LICENSE for licensing information

#pragma once

#include <signal.h>
#include <thread>
#include <unistd.h>
#include <cassert>
#include <iostream>

#include "barrier.h"
#include "timing.h"
#include "bmconfig.h"

/// A hack for making sure each thread goes to its own place
thread_local int thread_id;

/// The benchmark class provides a standard way of doing insert/lookup/remove
/// operations on a set
template<class SET>
class benchmark
{
    /// The data structure we will manipulate
    SET* set;

    /// A barrier for ensuring all threads move forward together
    barrier* thread_barrier;

    /// Each iteration of the test will decide whether to insert, lookup, or
    /// remove
    void test_iteration(uint32_t id, uint32_t* seed, int counts[]) {
        uint32_t val = rand_r(seed) % Config::CFG.elements;
        uint32_t act = rand_r(seed) % 100;
        bool res;
        if (act < Config::CFG.lookpct) {
            __transaction_atomic {
                res = set->lookup(val);
            }
            counts[res?0:1]++;
        }
        else if (act < Config::CFG.inspct) {
            __transaction_atomic {
                res = set->insert(val);
            }
            counts[res?2:3]++;
        }
        else {
            __transaction_atomic {
                res = set->remove(val);
            }
            counts[res?4:5]++;
        }
    }

    /// This code runs some no-ops between transactions, if requested
    void nontxnwork() {
        if (Config::CFG.nops_after_tx)
            for (uint32_t i = 0; i < Config::CFG.nops_after_tx; i++)
                __asm__ __volatile__("nop");
    }

    /// This oversees the repeated execution of test_iteration, which will be
    /// performed based on timing, or a fixed number of operations, depending
    /// on the configuration of this experiment
    void run(uintptr_t id) {
        // set thread id
        thread_id = id;
        // wait until all threads created, then set alarm and read timer
        thread_barrier->arrive(id);
        if (id == 0) {
            if (!Config::CFG.execute) {
                signal(SIGALRM, Config::catch_SIGALRM);
                alarm(Config::CFG.duration);
            }
            Config::CFG.time = getElapsedTime();
        }

        // wait until read of start timer finishes, then start transactions
        thread_barrier->arrive(id);
        // these are for successful lookups, failed lookups,
        // successful inserts, failed inserts, successful removes,
        // and failed removes
        int counts[6] = {0, 0, 0, 0, 0, 0};
        uint32_t count = 0;
        uint32_t seed = id; // not everyone needs a seed, but we have to support it
        if (!Config::CFG.execute) {
            // run txns until alarm fires
            while (Config::CFG.running) {
                test_iteration(id, &seed, counts);
                ++count;
                nontxnwork(); // some nontx work between txns?
            }
        }
        else {
            // run fixed number of txns
            for (uint32_t e = 0; e < Config::CFG.execute; e++) {
                test_iteration(id, &seed, counts);
                ++count;
                nontxnwork(); // some nontx work between txns?
            }
        }

        // wait until all txns finish, then get time
        thread_barrier->arrive(id);
        if (id == 0)
            Config::CFG.time = getElapsedTime() - Config::CFG.time;

        // add this thread's count to an accumulator
        Config::CFG.txcount += count;
        Config::CFG.lookup_hit  += counts[0];
        Config::CFG.lookup_miss += counts[1];
        Config::CFG.insert_hit  += counts[2];
        Config::CFG.insert_miss += counts[3];
        Config::CFG.remove_hit  += counts[4];
        Config::CFG.remove_miss += counts[5];
    }

    /// wrapper for running the experiments, since threads can't call methods
    /// directly, only functions
    static void run_wrapper(int i, benchmark<SET>* b) {
        b->run(i);
        b->thread_barrier->arrive(i);
        b->thread_barrier->arrive(i);
    }

  public:

    /// The constructor doesn't build a barrier, because we don't know the
    /// thread count yet
    benchmark() : set(new SET()), thread_barrier(NULL) { }

    /// An alternative constructor that takes a pre-constructed SET
    benchmark(SET* _set) : set(_set), thread_barrier(NULL) { }

    /// warm up the data structure in a repeatable way
    void warmup() {
        // warm up the datastructure
        for (int32_t w = Config::CFG.elements; w >= 0; w-=2)
            set->insert(w);
        assert(set->isSane());
    }

    /// Create threads and a barrier, then run the tests
    void launch_test() {
        if (thread_barrier != NULL)
            delete(thread_barrier);
        thread_barrier = new barrier(Config::CFG.threads);

        // kick off the threads (this thread runs too...)
        std::thread* threads = new std::thread[Config::CFG.threads];
        for (int i = 1; i < Config::CFG.threads; ++i)
            threads[i] = std::thread(run_wrapper, i, this);

        run_wrapper(0, this);

        // wait for completion
        for (int i = 1; i < Config::CFG.threads; ++i)
            threads[i].join();

        // test for correctness
        bool v = set->isSane();
        std::cout << "Verification: " << (v ? "Passed" : "Failed") << "\n";
    }
};
