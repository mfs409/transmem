/* =============================================================================
 *
 * coordinate.c
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


#include <math.h>
#include <stdio.h>
#include "coordinate.h"


/* =============================================================================
 * coordinate_compare
 * =============================================================================
 */
__attribute__ ((transaction_safe))
long
coordinate_compare (coordinate_t* aPtr, coordinate_t* bPtr)
{
    if (aPtr->x < bPtr->x) {
        return -1;
    } else if (aPtr->x > bPtr->x) {
        return 1;
    } else if (aPtr->y < bPtr->y) {
        return -1;
    } else if (aPtr->y > bPtr->y) {
        return 1;
    }

    return 0;
}

#define ABS(a) (((a) > 0) ? (a) : -(a))
/*
 * a transaction_safe version of sqrt, Newton's method
 */
__attribute__ ((transaction_safe))
double sqrt_safe ( double x )
{
  if (x == 0)
    return 0;
  double a = 10;
  double t = x / 100000;
  while (ABS(a * a - x) >= t) {
    a = a - (a * a - x) / (2 * a);
  }
  return a;
}


__attribute__ ((transaction_pure))
double sqrt_pure ( double x )
{
  return sqrt (x);
}

/* =============================================================================
 * coordinate_distance
 * =============================================================================
 */
__attribute__ ((transaction_safe))
double
coordinate_distance (coordinate_t* coordinatePtr, coordinate_t* aPtr)
{
    double delta_x = coordinatePtr->x - aPtr->x;
    double delta_y = coordinatePtr->y - aPtr->y;

    //[wer] need to be safe!
    //return sqrt((delta_x * delta_x) + (delta_y * delta_y));
    return sqrt_safe((delta_x * delta_x) + (delta_y * delta_y));
}
#define PI 3.141592653

__attribute__ ((transaction_safe))
double my_exp(double x, int y)
{
    double ret = 1;
    int i;
    for (i = 0; i < y; i++)
        ret *= x;
    return ret;
}

__attribute__ ((transaction_safe))
double acos_safe( double x)
{
    return PI / 2 - x - my_exp(x, 3) / 6
        - 3 * my_exp(x, 5) / 40
        - 5 * my_exp(x, 7) / 112
        - 35 * my_exp (x, 9) / 1152;
}

__attribute__ ((transaction_pure))
double acos_pure( double x)
{
  return acos(x);
}

/* =============================================================================
 * coordinate_angle
 *
 *           (b - a) .* (c - a)
 * cos a = ---------------------
 *         ||b - a|| * ||c - a||
 *
 * =============================================================================
 */
__attribute__ ((transaction_safe))
double
coordinate_angle (coordinate_t* aPtr, coordinate_t* bPtr, coordinate_t* cPtr)
{
    coordinate_t delta_b;
    coordinate_t delta_c;
    double distance_b;
    double distance_c;
    double numerator;
    double denominator;
    double cosine;
    double radian;

    delta_b.x = bPtr->x - aPtr->x;
    delta_b.y = bPtr->y - aPtr->y;

    delta_c.x = cPtr->x - aPtr->x;
    delta_c.y = cPtr->y - aPtr->y;

    numerator = (delta_b.x * delta_c.x) + (delta_b.y * delta_c.y);

    distance_b = coordinate_distance(aPtr, bPtr);
    distance_c = coordinate_distance(aPtr, cPtr);
    denominator = distance_b * distance_c;

    cosine = numerator / denominator;
    //[wer210][replace it with SAFE]
    //radian = acos(cosine);
    radian = acos_safe(cosine);

    return (180.0 * radian / M_PI);
}


/* =============================================================================
 * coordinate_print
 * =============================================================================
 */
void
coordinate_print (coordinate_t* coordinatePtr)
{
    printf("(%+0.4lg, %+0.4lg)", coordinatePtr->x, coordinatePtr->y);
}


#ifdef TEST_COORDINATE
/* /////////////////////////////////////////////////////////////////////////////
 * TEST_COORDINATE
 * /////////////////////////////////////////////////////////////////////////////
 */


#include <assert.h>
#include <stdio.h>


static void
printAngle (coordinate_t* coordinatePtr, coordinate_t* aPtr, coordinate_t* bPtr,
            double expectedAngle)
{
    double angle = coordinate_angle(coordinatePtr, aPtr, bPtr);

    printf("(%lf, %lf) \\ (%lf, %lf) / (%lf, %lf) -> %lf\n",
           aPtr->x ,aPtr->y,
           coordinatePtr->x, coordinatePtr->y,
           bPtr->x, bPtr->y,
           angle);

    assert((angle - expectedAngle) < 1e-6);
}


int
main (int argc, char* argv[])
{
    coordinate_t a;
    coordinate_t b;
    coordinate_t c;

    a.x = 0;
    a.y = 0;

    b.x = 0;
    b.y = 1;

    c.x = 1;
    c.y = 0;

    printAngle(&a, &b, &c, 90.0);
    printAngle(&b, &c, &a, 45.0);
    printAngle(&c, &a, &b, 45.0);

    c.x = sqrt(3);
    c.y = 0;

    printAngle(&a, &b, &c, 90.0);
    printAngle(&b, &c, &a, 60.0);
    printAngle(&c, &a, &b, 30.0);

    return 0;
}

#endif /* TEST_COORDINATE */


/* =============================================================================
 *
 * End of coordinate.c
 *
 * =============================================================================
 */
