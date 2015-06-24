// -*-c++-*-
//
//  Copyright (C) 2011, 2015
//  University of Rochester Department of Computer Science
//    and
//  Lehigh University Department of Computer Science and Engineering
//
// License: Modified BSD
//          Please see the file LICENSE for licensing information

#pragma once

#include <unistd.h>
#include <pthread.h>

/// sleep_ms simply wraps the POSIX usleep call.  Note that usleep expects a
/// number of microseconds, not milliseconds
inline void sleep_ms(uint32_t ms) { usleep(ms*1000); }

/// Yield the CPU via pthread_yield()
inline void yield_cpu() { pthread_yield(); }

/// The Linux clock_gettime is reasonably fast, has good resolution, and is
/// not affected by TurboBoost or DVFS.
inline uint64_t getElapsedTime() {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  uint64_t tt = (((long long)t.tv_sec) * 1000000000L) + ((long long)t.tv_nsec);
  return tt;
}
