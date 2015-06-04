#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := genome

#
# Basenames of .c files from this folder
#
SRCS = gene genome segments sequencer table

#
# Basenames of .c files from the lib/ folder
#
LIBSRCS = bitmap hash hashtable pair random list mt19937ar thread vector

#
# Extra flags
#
CFLAGS = -DLIST_NO_DUPLICATES -DCHUNK_STEP1=12

#
# Pull in the standard build rules
#
include ../Makefile.common
