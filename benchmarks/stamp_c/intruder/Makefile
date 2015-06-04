#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := intruder

#
# Basenames of .c files from this folder
#
SRCS = decoder detector dictionary intruder packet preprocessor stream

#
# Basenames of .c files from the lib/ folder
#
LIBSRCS = list mt19937ar pair queue random rbtree thread vector

#
# Extra flags
#
CFLAGS = -DMAP_USE_RBTREE

#
# Pull in the standard build rules
#
include ../Makefile.common
