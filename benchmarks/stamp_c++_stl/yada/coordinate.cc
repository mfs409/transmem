/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
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
coordinate_compare (const coordinate_t* aPtr, const coordinate_t* bPtr)
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
double sqrt_safe ( double x ) {
    if (x == 0)
        return 0;
    double a = 10;
    double t = x / 100000;
    while (ABS(a * a - x) >= t) {
        a = a - (a * a - x) / (2 * a);
    }
    return a;
}

__attribute__ ((transaction_safe))
double my_exp(double x, int y)
{
    double ret = 1;
    int i;
    for (i = 0; i < y; i++)
        ret *= x;
    return ret;
}

#define PI 3.141592653

__attribute__ ((transaction_safe))
double acos_safe( double x) {
    return PI / 2 - x - my_exp(x, 3) / 6
        - 3 * my_exp(x, 5) / 40
        - 5 * my_exp(x, 7) / 112
        - 35 * my_exp (x, 9) / 1152;
}


__attribute__ ((transaction_safe))
double coordinate_t::distance(coordinate_t* aPtr)
{
    double delta_x = x - aPtr->x;
    double delta_y = y - aPtr->y;

    //[wer] need to be safe!
    //return sqrt((delta_x * delta_x) + (delta_y * delta_y));
    return sqrt_safe((delta_x * delta_x) + (delta_y * delta_y));
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
double coordinate_t::angle(coordinate_t* bPtr, coordinate_t* cPtr)
{
    coordinate_t delta_b;
    coordinate_t delta_c;
    double distance_b;
    double distance_c;
    double numerator;
    double denominator;
    double cosine;
    double radian;

    delta_b.x = bPtr->x - x;
    delta_b.y = bPtr->y - y;

    delta_c.x = cPtr->x - x;
    delta_c.y = cPtr->y - y;

    numerator = (delta_b.x * delta_c.x) + (delta_b.y * delta_c.y);

    distance_b = distance(bPtr);
    distance_c = distance(cPtr);
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
void coordinate_t::print()
{
    printf("(%+0.4lg, %+0.4lg)", x, y);
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
