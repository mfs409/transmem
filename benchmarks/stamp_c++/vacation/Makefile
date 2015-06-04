#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := vacation

#
# Basenames of .cc files from this folder
#
SRCS = client customer manager reservation vacation

#
# Basenames of .cc files from the lib/ folder
#
LIBSRCS = list pair rbtree thread

#
# Extra flags
#
CXXFLAGS = -DLIST_NO_DUPLICATES -DMAP_USE_RBTREE

#
# Pull in the standard build rules
#
include ../Makefile.common
