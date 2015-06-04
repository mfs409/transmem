/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

struct coordinate_t {
    double x;
    double y;

  __attribute__ ((transaction_safe))
  double distance(coordinate_t* aPtr);

  __attribute__ ((transaction_safe))
  double angle(coordinate_t* aPtr, coordinate_t* bPtr);

  void print();
};

__attribute__ ((transaction_safe))
long coordinate_compare(const coordinate_t* aPtr, const coordinate_t* bPtr);


