#include "libitm_i.h"

namespace GTM HIDDEN {
  /**
   *  Specializations for adding each of libITM's 10 primitive types to the
   *  write log.
   */
  INSTANTIATE_BST(uint8_t);
  INSTANTIATE_BST(uint16_t);
  INSTANTIATE_BST(uint32_t);
  INSTANTIATE_BST(uint64_t);
  INSTANTIATE_BST(float);
  INSTANTIATE_BST(double);
  INSTANTIATE_BST(float _Complex);
  INSTANTIATE_BST(double _Complex);
  INSTANTIATE_BST(long double);
  INSTANTIATE_BST(long double _Complex);

} // namespace GTM
