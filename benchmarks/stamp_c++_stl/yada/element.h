/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <utility>
#include <set>
#include "coordinate.h"

typedef std::pair<coordinate_t*, coordinate_t*> edge_t;

struct element_t;
struct element_listCompare_t
{
    __attribute__((transaction_safe))
    bool operator()(const element_t* aPtr, const element_t* bPtr);
};

struct element_t {
    coordinate_t coordinates[3];
    long numCoordinate;
    coordinate_t circumCenter;
    double circumRadius;
    double minAngle;
    edge_t edges[3];
    long numEdge;
    coordinate_t midpoints[3]; /* midpoint of each edge */
    double radii[3];           /* half of edge length */
    edge_t* encroachedEdgePtr; /* opposite obtuse angle */
    bool isSkinny;
    std::set<element_t*, element_listCompare_t>* neighborListPtr;
    bool isGarbage;
    bool isReferenced;

  __attribute__((transaction_safe))
  element_t(coordinate_t* coordinates, long numCoordinate);

  __attribute__((transaction_safe))
  ~element_t();

  __attribute__((transaction_safe))
  long getNumEdge();

  /*
   * element_getEdge: Returned edgePtr is sorted; i.e., coordinate_compare(first, second) < 0
   */
  __attribute__((transaction_safe))
  edge_t* getEdge(long i);

  __attribute__((transaction_safe))
  bool isInCircumCircle(coordinate_t* coordinatePtr);

  __attribute__((transaction_safe))
  void clearEncroached();

  __attribute__((transaction_safe))
  edge_t* getEncroachedPtr();

  bool isEltSkinny();

  /*
   * element_isBad: Does it need to be refined?
   */
  __attribute__((transaction_safe))
  bool isBad();

  __attribute__((transaction_safe))
  void setIsReferenced(bool status);

  /*
   * TMelement_isGarbage: Can we deallocate?
   */
  __attribute__((transaction_safe))
  bool isEltGarbage();

  __attribute__((transaction_safe))
  void setIsGarbage(bool status);

  __attribute__((transaction_safe))
  void addNeighbor(element_t* neighborPtr);

  __attribute__((transaction_safe))
  std::set<element_t*, element_listCompare_t>* getNeighborListPtr();

  /*
   * element_getNewPoint:  Either the element is encroached or is skinny, so get the new point to add
   */
  //[wer210] previous returns a struct, which causes errors
  // coordinate_t element_getNewPoint(element_t* elementPtr);
  __attribute__((transaction_safe))
  void getNewPoint(coordinate_t* ret);

  /*
   * element_checkAngles: Return false if minimum angle constraint not met
   */
  bool eltCheckAngles();

  void print();

  void printAngles();
};

__attribute__((transaction_safe))
long element_compare(const element_t* aElementPtr, const element_t* bElementPtr);

__attribute__((transaction_safe))
long element_listCompare(const void* aPtr, const void* bPtr);

__attribute__((transaction_safe))
long element_listCompareEdge(const void* aPtr, const void* bPtr);

__attribute__((transaction_safe))
long element_heapCompare(const void* aPtr, const void* bPtr);

__attribute__((transaction_safe))
edge_t* element_getCommonEdge(element_t* aElementPtr, element_t* bElementPtr);

void element_printEdge(edge_t* edgePtr);

struct element_listCompareEdge_t
{
  bool operator()(const edge_t* aPtr, const edge_t* bPtr)
  {
    return element_listCompareEdge(aPtr, bPtr) < 0;
  }
};

struct element_mapCompareEdge_t
{
  __attribute__((transaction_safe))
  bool operator()(const edge_t* left, const edge_t* right);
};

struct element_mapCompare_t
{
  bool operator()(const element_t* aPtr, const element_t* bPtr)
  {
    return element_compare(aPtr, bPtr) < 0;
  }
};

struct element_heapCompare_t
{
    __attribute__((transaction_safe))
    bool operator()(const element_t* aPtr, const element_t* bPtr);
};
