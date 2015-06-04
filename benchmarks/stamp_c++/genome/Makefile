#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := genome

#
# Basenames of .cc files from this folder
#
SRCS = gene genome segments sequencer table

#
# Basenames of .cc files from the lib/ folder
#
LIBSRCS = bitmap hash hashtable pair list thread vector

#
# Extra flags
#
CXXFLAGS = -DLIST_NO_DUPLICATES -DCHUNK_STEP1=12

#
# Pull in the standard build rules
#
include ../Makefile.common
