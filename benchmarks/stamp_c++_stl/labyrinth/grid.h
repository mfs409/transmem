/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <vector>

#define GRID_POINT_FULL  (-2L)
#define GRID_POINT_EMPTY (-1L)

struct grid_t {
    long width;
    long height;
    long depth;
    long* points;
    long* points_unaligned;

    grid_t(long width, long height, long depth);

    ~grid_t();

    __attribute__((transaction_safe))
    bool isPointValid(long x, long y, long z);

    __attribute__((transaction_safe))
    long* getPointRef(long x, long y, long z);

    __attribute__((transaction_safe))
    void getPointIndices(long* gridPointPtr, long* xPtr, long* yPtr, long* zPtr);

    __attribute__((transaction_safe))
    long getPoint(long x, long y, long z);

    __attribute__((transaction_safe))
    bool isPointEmpty(long x, long y, long z);

    __attribute__((transaction_safe))
    bool isPointFull(long x, long y, long z);

    __attribute__((transaction_safe))
    void setPoint(long x, long y, long z, long value);

    void addPath(std::vector<coordinate_t*>* pointVectorPtr);

    __attribute__((transaction_safe))
    bool TMaddPath(std::vector<long*>* pointVectorPtr);

    void print();
};

void grid_copy(grid_t* dstGridPtr, grid_t* srcGridPtr);
