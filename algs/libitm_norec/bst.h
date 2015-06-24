#ifndef BST_HPP__
#define BST_HPP__

namespace GTM HIDDEN
{
  /**
   *  An (unbalanced) binary search tree specialized for our writeset needs
   *
   *  Each node in this bst map consists of a 64-byte slab of data, the address
   *  of the first byte (the key), and a bitmask to show which bytes have
   *  actually been written.
   *
   *  Since we want to be able to quickly clear the bst, avoid dynamic memory
   *  allocation, and have good cache performance, we separate the node (child
   *  pointers, key, mask) and the payload.  This lets the payload slab be
   *  exactly 64 bytes, and it lets the node size be a multiple of 4/8
   *  depending on whether we are compiling for 32 or 64 bit code.  We also do
   *  not use pointers, but instead integral indices.  In this manner, we can
   *  have a pool of nodes, and a pool of slabs, and then refer to the next
   *  node/slab by its index.  When the pool is exhausted, we can simply and
   *  efficiently realloc() it, and the indices do not need to change.
   *
   *  [transmem] we should try to use a balanced tree or hash table instead.
   */
  class BST
  {
      /**
       *  Size of a slab... we use 64 bytes for now, because GCC seems to work
       *  well with 64-byte alignment, and because anything larger would mean
       *  that node_t::mask couldn't be a primitive type.
       */
      static const size_t SLAB_SIZE = 64;

      /**
       *  Each slab is a 64-byte array, which represents new values for 64
       *  contiguous bytes of memory.  For alignment purposes, we keep the slab
       *  separate from its starting address and mask.
       */
      struct slab_t
      {
          uint8_t data[SLAB_SIZE]; // the data
      };

      /**
       *  The node consists of left and right pointers, a key (64-bit aligned
       *  address) and a 64-bit mask of which bytes in the corresponding slab
       *  are live.  Note that there is no slab pointer.  The index of a slab
       *  and the index of a node should correlate
       */
      struct node_t
      {
          int       left_idx;     // index of left child
          int       right_idx;    // index of right child
          uintptr_t key;          // key stored here
          uint64_t  mask;         // mask of valid bits in slab

          /**
           *  We need to re-initialize a node whenever we get one from the
           *  pool.  We do this by setting the key, nulling out pointers, and
           *  zeroing the mask
           */
          void reinit(uintptr_t k)
          {
              left_idx = right_idx = -1;
              key = k;
              mask = 0;
          }
      };

      /**
       *  The node pool consists of a pointer to a set of node objects, an int
       *  representing the number of nodes in the pool, and an int representing
       *  the index of the next node to allocate.
       *
       *  Note that the one-to-one correspondence between slabs and nodes means
       *  only one next field, and one size field, are needed
       */
      node_t* nodepool;

      /**
       *  The slab pool consists of a pointer to a set of slab objects, an int
       *  representing the number of slabs in the pool, and an int representing
       *  the index of the next slab to allocate.
       *
       *  See note above... we don't need a separate size or next pointer for
       *  the slab pool
       */
      slab_t* slabpool;

      /**
       *  The next free node/slab in the pool
       */
      int pool_next;

      /**
       *  The size of the node and slab pools
       */
      int pool_size;

      /**
       *  The index of the root node.
       *
       *  [transmem] This is either 0 or -1 for now, though it might change
       *             when we allow rebalancing
       */
      int root_idx;

      /**
       *  The initial size of the pools
       */
      static const size_t INITIAL_SIZE = 1024;

      /**
       *  This function takes a key, and returns the index of the node and slab
       *  that correspond to that key.  If the key is not in the data
       *  structure, then a new slab and node will be created for the key.  The
       *  point is to say "get me the index of the slab into which my data
       *  should go, where my data's address, with least significant 64 bits
       *  masked out, is the key.
       *
       *  Returns the index of the slab where the key belongs...
       */
      int reserve(uintptr_t key)
      {
          // if tree is empty, make a new root
          if (isEmpty()) {
              // no need to check for space, just grab the first entry,
              // initialize it, and make it the root.
              int my_idx = pool_next++;
              nodepool[my_idx].reinit(key);
              root_idx = my_idx;
              return my_idx;
          }

          // if we can find this key in the tree, we don't need to return it...
          int curr = root_idx;
          int parent;
          while (curr != -1) {
              parent = curr;
              // if the key matches this entry, then return an index to this
              // entry.  Otherwise traverse to a (possibly null) child
              if (nodepool[curr].key == key)
                  return curr;
              if (key < nodepool[curr].key)
                  curr = nodepool[curr].left_idx;
              else
                  curr = nodepool[curr].right_idx;
          }

          // if we are here, then we have a parent, and need a new node.  First
          // make sure the pools aren't full
          if (pool_next == pool_size) {
              pool_size *= 2;
              //nodepool = (node_t*)realloc(nodepool, pool_size);
              node_t* nodepool1 = (node_t*)malloc (pool_size * sizeof(node_t));
              memcpy(nodepool1, nodepool, sizeof(node_t) * pool_size / 2);
              free(nodepool);
              nodepool = nodepool1;

              //slabpool = (slab_t*)realloc(slabpool, pool_size);
              slab_t* slabpool1 = (slab_t*)malloc (pool_size * sizeof(slab_t));
              memcpy(slabpool1, slabpool, sizeof(slab_t) * pool_size / 2);
              free(slabpool);
              slabpool = slabpool1;
          }

          // reserve a position from the pools, attach it in the right place,
          // and return the position's index
          int new_node = pool_next++;
          nodepool[new_node].reinit(key);
          if (key < nodepool[parent].key)
              nodepool[parent].left_idx = new_node;
          else
              nodepool[parent].right_idx = new_node;
          return new_node;
    }

      /**
       *  Return the index of the node that holds this key, or -1 on failure
       */
      int lookup(uintptr_t key)
      {
          // start at root
          int curr = root_idx;

          while (curr != -1) {
              // if node matches, return true
              if (nodepool[curr].key == key)
                  return curr;
              // go left or right depending on value
              if (key < nodepool[curr].key)
                  curr = nodepool[curr].left_idx;
              else
                  curr = nodepool[curr].right_idx;
        }
          return -1;
      }

    public:

      /**
       *  initially the tree has no root, and two well-defined pools.
       */
      BST()
      {
          root_idx = -1;

          pool_size = INITIAL_SIZE;
          pool_next = 0;

          //slabpool = new slab_t[pool_size]();
          //nodepool = new node_t[pool_size]();
          slabpool = (slab_t*) calloc(pool_size, sizeof(slab_t));
          nodepool = (node_t*) calloc(pool_size, sizeof(node_t));
      }


      ~BST()
      {
          free(slabpool);
          free(nodepool);
      }

      /**
       *  Return whether the tree is empty or not... this is useful in the
       *  commit function.
       */
      bool isEmpty() const
      {
        return root_idx == -1;
      }

      /**
       *  Since we manually manage memory via pools, we can reset the data
       *  structure with two stores that make the root invalid and reset the
       *  pools.
       *
       *  Note that we don't resize the pools... if they grew, we assume we'll
       *  have another transaction in the future that also needs larger pools.
       */
      void reset()
      {
          root_idx  = -1;
          pool_next = 0;
      }

      /**
       *  Method for inserting an element to the write set, by type.
       *
       *  NB: This assumes that the datum does not span a 64-byte boundary
       */
      template <typename T>
      void insert(const T* addr, T val);

      /**
       *  Method for Commutatively inserting an element to the write set, by type.
       *
       *  NB: This assumes that the datum does not span a 64-byte boundary
       */
      template <typename T>
      void commu_insert(const T* addr, T val);

      /**
       *  Method for finding an element in the write set
       *
       *  This return the mask that describes which bits of val are valid
       *
       *  NB: Again, assumes that the datum does not span a 64-byte boundary
       */
      template <typename T>
      int find(const T* addr, T& val);

      template <typename T>
      int find_addr(const T* addr);

      /**
       *  Method for removing an element in the write set
       *
       *  This should only used in opslog, as it only zero out the cooresponding
       *  bit, but not actually remove the node
       */
      template <typename T>
      int remove(const T* addr, T& val);

      /**
       *  Method for doing writeback
       */
      void writeback()
      {
          // iterate through the slabs, and then write out the bytes
          for (int i = 0; i < pool_next; ++i) {
              for (int bytes = 0; bytes < 64; bytes += 4) {
                  // figure out if current 4 bytes are all valid
                  int m = nodepool[i].mask >> bytes;
                  m = m & 0xF;
                  if (m == 0xF) {
                      // we can write this as a 32-bit word
                      uint32_t* addr = (uint32_t*)(nodepool[i].key + bytes);
                      uint32_t* data = (uint32_t*)(slabpool[i].data + bytes);
                      *addr = *data;
                  }
                  else if (m != 0) {
                      // write out live bytes, one at a time
                      uint8_t* addr = (uint8_t*)nodepool[i].key + bytes;
                      uint8_t* data = slabpool[i].data + bytes;
                      // [transmem] Verify that we're better off
                      // with an easily unrolled loop than a while loop
                      for (int q = 0; q < 4; ++q) {
                          if (m & 1)
                              *addr = *data;
                          addr++;
                          data++;
                          m >>= 1;
                      }
                  }
              }
          }
      }

      /**
       *  Method for handling rollback of an exception object
       *
       *  [transmem] If GCC instruments writes to an exception object, then
       *             we need this, otherwise not.  For now, leave it
       *             unimplemented.
       */
      void rollback();

      /**
       *  Report whether a realloc will occur on the next new insertion.  We
       *  used this in some of our ASF HTM codes.
       */
      bool will_reorg()
      {
          return pool_next == pool_size;
      }

      /**
       *  We need to somehow iterate over addresses in a manner that lets us
       *  know what locks to acquire.  We support this via a two-step
       *  interface.  First, we allow a report of the number of slabs.  Then,
       *  we allow querying by slab ID to get the key and mask.  Everything
       *  else is up to the user to do.
       */

      /**
       *  Return the number of active slabs
       */
      int slabcount()
      {
          return pool_next;
      }

      /**
       *  Allow queries to see the mask for a given slab
       */
      uint64_t get_mask(int slab_id)
      {
          return nodepool[slab_id].mask;
      }

      /**
       *  Allow queries to see the key for a given slab
       */
      uintptr_t get_key(int slab_id)
      {
          return nodepool[slab_id].key;
      }

  };

  /**
   *  In gcc's libitm, we have 10 primitive types:
   *
   *  |----------------------+--------------+---------------|
   *  | real type            | ITM name     | size32/size64 |
   *  |----------------------+--------------+---------------|
   *  | uint8_t              | _ITM_TYPE_U1 | 1 byte        |
   *  | uint16_t             | _ITM_TYPE_U2 | 2 bytes       |
   *  | uint32_t             | _ITM_TYPE_U4 | 4 bytes       |
   *  | uint64_t             | _ITM_TYPE_U8 | 8 bytes       |
   *  | float                | _ITM_TYPE_F  | 4 bytes       |
   *  | double               | _ITM_TYPE_D  | 8 bytes       |
   *  | long double          | _ITM_TYPE_E  | 12 / 16 bytes |
   *  | float _Complex       | _ITM_TYPE_CF | 8 bytes       |
   *  | double _Complex      | _ITM_TYPE_CD | 16 bytes      |
   *  | long double _Complex | _ITM_TYPE_CE | 24 / 32 bytes |
   *  |----------------------+--------------+---------------|
   *
   *  Of these, long double and long double _Complex are the only ones that
   *  require different specializations for 32/64-bit mode.
   *
   *  Note that we are going to assume that no read/write spans a 64 byte
   *  boundary.  Any good cross-platform code will also provide the stronger
   *  guarantee that we do not read/write across a datum-aligned boundary, but
   *  we don't check for that.
   *
   *  [transmem] This code is probably not correct for big endian machines
   */

  /**
   *  Rather than write the same boilerplate code 12 times, we'll write macros
   *  that can be adapted for use in each of the 12 instances.  The only
   *  differences should be in the type (T) and the mask (M)
   */

  /**
   *  Instantiate the code for adding to the BST, commutativly
   */

#define INSTANTIATE_BST_COMM_INS(T, M)                       \
  template <>                                                \
  void BST::commu_insert(const T* addr, T val)               \
  {                                                          \
      assert(false);                                         \
                                                             \
      /* determine key (64-bit aligned address) */           \
      uintptr_t key = (uintptr_t)addr & ~0x3FLL;             \
                                                             \
      /* determine offset of this address within block */    \
      uint64_t offset = (uintptr_t)addr & 0x3F;              \
                                                             \
      /* use key to get index of target slab */              \
      int idx = reserve(key);                                \
                                                             \
      /* get naked addr of location to update in slab */     \
      uint8_t* dataptr = slabpool[idx].data;                 \
      dataptr += offset;                                     \
                                                             \
      /* convert to proper type, update location */          \
      T* tgt = (T*)dataptr;                                  \
      *tgt += val;                                           \
                                                             \
      /* update the mask */                                  \
      uint64_t mask = M;                                     \
      nodepool[idx].mask |= (mask << offset);                \
  }

  /**
   *  Instantiate the code for adding to the BST
   */

#define INSTANTIATE_BST_INS(T, M)                            \
  template <>                                                \
  void BST::insert(const T* addr, T val)                     \
  {                                                          \
      /* determine key (64-bit aligned address) */           \
      uintptr_t key = (uintptr_t)addr & ~0x3FLL;             \
                                                             \
      /* determine offset of this address within block */    \
      uint64_t offset = (uintptr_t)addr & 0x3F;              \
                                                             \
      /* use key to get index of target slab */              \
      int idx = reserve(key);                                \
                                                             \
      /* get naked addr of location to update in slab */     \
      uint8_t* dataptr = slabpool[idx].data;                 \
      dataptr += offset;                                     \
                                                             \
      /* convert to proper type, update location */          \
      T* tgt = (T*)dataptr;                                  \
      *tgt = val;                                            \
                                                             \
      /* update the mask */                                  \
      uint64_t mask = M;                                     \
      nodepool[idx].mask |= (mask << offset);                \
  }


  /**
   *  Instantiate the code for doing a lookup in the bst
   */
#define INSTANTIATE_BST_FIND(T, M)                            \
  template <>                                                 \
  int BST::find(const T* addr, T& val)                        \
  {                                                           \
      /* determine key (64-bit aligned address) */            \
      uintptr_t key = (uintptr_t)addr & ~0x3FLL;              \
                                                              \
      /* determine offset of this address within block */     \
      uint64_t offset = (uintptr_t)addr & 0x3F;               \
                                                              \
      /* use key to get index of target slab */               \
      int idx = lookup(key);                                  \
                                                              \
      /* if no slab, then we're done */                       \
      if (idx == -1)                                          \
          return 0;                                           \
                                                              \
      /* if our bytes in slab not set, then we're done */     \
      uint64_t mask = M;                                      \
      uint64_t nodemask = nodepool[idx].mask >> offset;       \
      uint32_t livebits = mask & nodemask;                    \
      if (!livebits)                                          \
          return 0;                                           \
                                                              \
      /* we have some live bits... get their address */       \
      uint8_t* dataptr = slabpool[idx].data;                  \
      dataptr += offset;                                      \
      T* tgt = (T*)dataptr;                                   \
                                                              \
      /* dereference to update val, return mask */            \
      val = *tgt;                                             \
      return livebits;                                        \
  }

  /**
   *  Instantiate the code for doing a lookup in the bst
   *  Only return True or False
   */
#define INSTANTIATE_BST_FIND_ADDR(T, M)                       \
  template <>                                                 \
  int BST::find_addr(const T* addr)                           \
  {                                                           \
      /* determine key (64-bit aligned address) */            \
      uintptr_t key = (uintptr_t)addr & ~0x3FLL;              \
                                                              \
      /* determine offset of this address within block */     \
      uint64_t offset = (uintptr_t)addr & 0x3F;               \
                                                              \
      /* use key to get index of target slab */               \
      int idx = lookup(key);                                  \
                                                              \
      /* if no slab, then we're done */                       \
      if (idx == -1)                                          \
          return 0;                                           \
                                                              \
      /* if our bytes in slab not set, then we're done */     \
      uint64_t mask = M;                                      \
      uint64_t nodemask = nodepool[idx].mask >> offset;       \
      uint32_t livebits = mask & nodemask;                    \
      if (!livebits)                                          \
          return 0;                                           \
      return 1;                                               \
  }

    /**
   *  Instantiate the code for doing a remove(zero it) in the bst
   */
#define INSTANTIATE_BST_REMOVE(T, M)                          \
  template <>                                                 \
  int BST::remove(const T* addr, T& val)                      \
  {                                                           \
      /* determine key (64-bit aligned address) */            \
      uintptr_t key = (uintptr_t)addr & ~0x3FLL;              \
                                                              \
      /* determine offset of this address within block */     \
      uint64_t offset = (uintptr_t)addr & 0x3F;               \
                                                              \
      /* use key to get index of target slab */               \
      int idx = lookup(key);                                  \
                                                              \
      /* if no slab, then we're done */                       \
      if (idx == -1)                                          \
          return 0;                                           \
                                                              \
      /* if our bytes in slab not set, then we're done */     \
      uint64_t mask = M;                                      \
      uint64_t nodemask = nodepool[idx].mask >> offset;       \
      uint32_t livebits = mask & nodemask;                    \
      if (!livebits)                                          \
          return 0;                                           \
                                                              \
      /* we have some live bits... get their address */       \
      uint8_t* dataptr = slabpool[idx].data;                  \
      dataptr += offset;                                      \
      T* tgt = (T*)dataptr;                                   \
                                                              \
      /* dereference to update val, return mask */            \
      val = *tgt;                                             \
                                                              \
      /* zero it */                                           \
      *tgt = 0;                                               \
      return livebits;                                        \
  }

  /**
   *  This macro does both instantiations for any given type.  Note that it
   *  automatically generates the correct mask based on the size of the type,
   *  and thus it gets the different masks and sizes right for 32-bit and
   *  64-bit code.
   */
#define INSTANTIATE_BST(T)                              \
  INSTANTIATE_BST_INS(T, (1UL<<sizeof(T)) - 1);         \
  INSTANTIATE_BST_FIND(T, (1UL<<sizeof(T)) - 1);        \
  INSTANTIATE_BST_FIND_ADDR(T, (1UL<<sizeof(T)) - 1);   \
  INSTANTIATE_BST_COMM_INS(T, (1UL<<sizeof(T)) - 1);    \
  INSTANTIATE_BST_REMOVE(T, (1UL<<sizeof(T)) - 1)

} // namespace GTM HIDDEN
#endif // BST_HPP__
