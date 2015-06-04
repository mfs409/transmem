#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := ssca2

#
# Basenames of .cc files from this folder
#
SRCS = alg_radix_smp computeGraph createPartition cutClusters findSubGraphs \
       genScalData getStartLists getUserParameters globals ssca2

#
# Basenames of .cc files from the lib/ folder
#
LIBSRCS = thread

#
# Extra flags
#
CXXFLAGS   = -DENABLE_KERNEL1
LDFLAGS    = -lm
#CXXFLAGS += -DUSE_PARALLEL_DATA_GENERATION
#CXXFLAGS += -DWRITE_RESULT_FILES
#CXXFLAGS += -DENABLE_KERNEL2 -DENABLE_KERNEL3
#CXXFLAGS += -DENABLE_KERNEL4

#
# Pull in the standard build rules
#
include ../Makefile.common
