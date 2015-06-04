#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := bayes

#
# Basenames of .c files from this folder
#
SRCS = adtree bayes data learner net sort

#
# Basenames of .c files from the lib/ folder
#
LIBSRCS = bitmap list mt19937ar queue random thread vector

#
# Extra flags
#
CFLAGS  = -DLIST_NO_DUPLICATES -DLEARNER_TRY_REMOVE -DLEARNER_TRY_REVERSE
LDFLAGS = -lm

#
# Pull in the standard build rules
#
include ../Makefile.common
