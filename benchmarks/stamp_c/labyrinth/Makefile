#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := labyrinth

#
# Basenames of .c files from this folder
#
SRCS = coordinate grid labyrinth maze router

#
# Basenames of .c files from the lib/ folder
#
LIBSRCS = list mt19937ar pair queue random thread vector

#
# Extra flags
#
CFLAGS  = -DUSE_EARLY_RELEASE
LDFLAGS = -lm

#
# Pull in the standard build rules
#
include ../Makefile.common

