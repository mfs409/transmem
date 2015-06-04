/**
 * The purpose of this file is to handle features that are transitioning to
 * transaction safety, but which aren't there yet in the GCC TM
 * implementation
 */

#pragma once

extern
__attribute__((transaction_pure))
void __assert_fail (const char *__assertion, const char *__file,
                           unsigned int __line, const char *__function)
  __THROW __attribute__ ((__noreturn__));
