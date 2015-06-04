#
# The name of the program we're building, and also the name of the
# subdirectory of the output folder in which this will be built.
#
PROG := ssca2

#
# Basenames of .c files from this folder
#
SRCS = alg_radix_smp computeGraph createPartition cutClusters findSubGraphs \
       genScalData getStartLists getUserParameters globals ssca2

#
# Basenames of .c files from the lib/ folder
#
LIBSRCS = mt19937ar random thread

#
# Extra flags
#
CFLAGS   = -DENABLE_KERNEL1
LDFLAGS  = -lm
#CFLAGS += -DUSE_PARALLEL_DATA_GENERATION
#CFLAGS += -DWRITE_RESULT_FILES
#CFLAGS += -DENABLE_KERNEL2 -DENABLE_KERNEL3
#CFLAGS += -DENABLE_KERNEL4

#
# Pull in the standard build rules
#
include ../Makefile.common
