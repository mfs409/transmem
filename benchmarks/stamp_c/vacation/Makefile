#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := vacation

#
# Basenames of .c files from this folder
#
SRCS = client customer manager reservation vacation

#
# Basenames of .c files from the lib/ folder
#
LIBSRCS = list pair mt19937ar random rbtree thread

#
# Extra flags
#
CFLAGS = -DLIST_NO_DUPLICATES -DMAP_USE_RBTREE

#
# Pull in the standard build rules
#
include ../Makefile.common
