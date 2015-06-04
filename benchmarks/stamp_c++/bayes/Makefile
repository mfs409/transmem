#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := bayes

#
# Basenames of .cc files from this folder
#
SRCS = adtree bayes data learner net sort

#
# Basenames of .cc files from the lib/ folder
#
LIBSRCS = bitmap list queue thread vector

#
# Extra flags
#
CXXFLAGS = -DLIST_NO_DUPLICATES -DLEARNER_TRY_REMOVE -DLEARNER_TRY_REVERSE
LDFLAGS  = -lm

#
# Pull in the standard build rules
#
include ../Makefile.common
