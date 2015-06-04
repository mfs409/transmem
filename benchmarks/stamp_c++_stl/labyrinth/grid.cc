/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "coordinate.h"
#include "grid.h"

/* ??? Cacheline size is fixed (set to 64 bytes for x86_64). */
const unsigned long CACHE_LINE_SIZE = 64UL;


/* =============================================================================
 * grid_alloc
 * =============================================================================
 */
grid_t::grid_t(long _width, long _height, long _depth)
{
    width  = _width;
    height = _height;
    depth  = _depth;
    long n = width * height * depth;
    points_unaligned = (long*)malloc(n * sizeof(long) + CACHE_LINE_SIZE);
    assert(points_unaligned);
    points = (long*)((char*)(((unsigned long)points_unaligned
                              & ~(CACHE_LINE_SIZE-1)))
                     + CACHE_LINE_SIZE);
    memset(points, GRID_POINT_EMPTY, (n * sizeof(long)));
}


/* =============================================================================
 * grid_free
 * =============================================================================
 */
grid_t::~grid_t()
{
    free(points_unaligned);
}


/* =============================================================================
 * grid_copy
 * =============================================================================
 */
void grid_copy (grid_t* dstGridPtr, grid_t* srcGridPtr)
{
    assert(srcGridPtr->width  == dstGridPtr->width);
    assert(srcGridPtr->height == dstGridPtr->height);
    assert(srcGridPtr->depth  == dstGridPtr->depth);

    long n = srcGridPtr->width * srcGridPtr->height * srcGridPtr->depth;
    memcpy(dstGridPtr->points, srcGridPtr->points, (n * sizeof(long)));

}


/* =============================================================================
 * grid_isPointValid
 * =============================================================================
 */
__attribute__((transaction_safe))
bool grid_t::isPointValid(long x, long y, long z)
{
    if (x < 0 || x >= width  || y < 0 || y >= height || z < 0 || z >= depth) {
        return false;
    }
    return true;
}


/* =============================================================================
 * grid_getPointRef
 * =============================================================================
 */
__attribute__((transaction_safe))
long*
grid_t::getPointRef(long x, long y, long z)
{
    return &(points[(z * height + y) * width + x]);
}


/* =============================================================================
 * grid_getPointIndices
 * =============================================================================
 */
__attribute__((transaction_safe))
void grid_t::getPointIndices(long* gridPointPtr, long* xPtr, long* yPtr, long* zPtr)
{
    long area = height * width;
    long index3d = (gridPointPtr - points);
    (*zPtr) = index3d / area;
    long index2d = index3d % area;
    (*yPtr) = index2d / width;
    (*xPtr) = index2d % width;
}


/* =============================================================================
 * grid_getPoint
 * =============================================================================
 */
__attribute__((transaction_safe))
long grid_t::getPoint(long x, long y, long z)
{
    return *getPointRef(x, y, z);
}


/* =============================================================================
 * grid_isPointEmpty
 * =============================================================================
 */
__attribute__((transaction_safe))
bool grid_t::isPointEmpty(long x, long y, long z)
{
    long value = getPoint(x, y, z);
    return ((value == GRID_POINT_EMPTY) ? true : false);
}


/* =============================================================================
 * grid_isPointFull
 * =============================================================================
 */
__attribute__((transaction_safe))
bool grid_t::isPointFull(long x, long y, long z)
{
    long value = getPoint(x, y, z);
    return ((value == GRID_POINT_FULL) ? true : false);
}

/* =============================================================================
 * grid_setPoint
 * =============================================================================
 */
__attribute__((transaction_safe))
void grid_t::setPoint(long x, long y, long z, long value)
{
    (*getPointRef(x, y, z)) = value;
}


/* =============================================================================
 * grid_addPath
 * =============================================================================
 */
void grid_t::addPath(std::vector<coordinate_t*>* pointVectorPtr)
{
    long i;
    long n = pointVectorPtr->size();

    for (i = 0; i < n; i++) {
        coordinate_t* coordinatePtr = pointVectorPtr->at(i);
        long x = coordinatePtr->x;
        long y = coordinatePtr->y;
        long z = coordinatePtr->z;
        setPoint(x, y, z, GRID_POINT_FULL);
    }
}


/* =============================================================================
 * TMgrid_addPath
 * =============================================================================
 */
__attribute__((transaction_safe))
//void
bool grid_t::TMaddPath(std::vector<long*>* pointVectorPtr)
{
    long i;
    long n = pointVectorPtr->size();
#if 0
    for (i = 1; i < (n-1); i++) {
        long* gridPointPtr = (long*)vector_at(pointVectorPtr, i);
        long value = (long)TM_SHARED_READ(*gridPointPtr);
        if (value != GRID_POINT_EMPTY) {
          _ITM_abortTransaction(2);
        }
        TM_SHARED_WRITE(*gridPointPtr, GRID_POINT_FULL);
    }
#endif
    //[wer210] a check loop and a write loop
    for (i = 1; i < (n-1); i++) {
        long* gridPointPtr = pointVectorPtr->at(i);
      long value = *gridPointPtr;
      if (value != GRID_POINT_EMPTY) {
        return false;
      }
      //TM_SHARED_WRITE(*gridPointPtr, (long)GRID_POINT_FULL);
    }

    for (i = 1; i < (n-1); i++) {
        long* gridPointPtr = pointVectorPtr->at(i);
        *gridPointPtr = (long)GRID_POINT_FULL;
    }
    return true;
}


/* =============================================================================
 * grid_print
 * =============================================================================
 */
void grid_t::print()
{
    for (long z = 0; z < depth; z++) {
        printf("[z = %li]\n", z);
        for (long x = 0; x < width; x++) {
            for (long y = 0; y < height; y++) {
                printf("%4li", *getPointRef(x, y, z));
            }
            puts("");
        }
        puts("");
    }
}
