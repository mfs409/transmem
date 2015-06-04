#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := intruder

#
# Basenames of .cc files from this folder
#
SRCS = decoder detector dictionary intruder packet preprocessor stream

#
# Basenames of .cc files from the lib/ folder
#
LIBSRCS = list pair queue rbtree thread vector

#
# Extra flags
#
CXXFLAGS = -DMAP_USE_RBTREE

#
# Pull in the standard build rules
#
include ../Makefile.common
