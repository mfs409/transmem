#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := yada

#
# Basenames of .cc files from this folder
#
SRCS = coordinate element mesh region yada

#
# Basenames of .cc files from the lib/ folder
#
LIBSRCS = avltree heap list pair queue rbtree thread vector

#
# Extra flags
#
CXXFLAGS = -DLIST_NO_DUPLICATES -DMAP_USE_AVLTREE -DSET_USE_RBTREE
LDFLAGS  = -lm

#
# Pull in the standard build rules
#
include ../Makefile.common
