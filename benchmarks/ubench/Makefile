#
# Files to compile that don't have a main() function
#
CXXFILES = Tree List

#
# Files to compile that do have a main() function
#
TARGETS = StdSetBench TreeBench ListBench CounterBench HashBench

#
# Let the user choose 32-bit or 64-bit compilation, but default to 32
#
BITS ?= 32

#
# Directory Names
#
ODIR          := ./obj$(BITS)
output_folder := $(shell mkdir -p $(ODIR))

#
# Tools
#
CXX = g++
LD  = g++

#
# Names of files that the compiler generates
#
EXEFILES  = $(patsubst %, $(ODIR)/%,   $(TARGETS))
OFILES    = $(patsubst %, $(ODIR)/%.o, $(CXXFILES))
EXEOFILES = $(patsubst %, $(ODIR)/%.o, $(TARGETS))
DEPS      = $(patsubst %, $(ODIR)/%.d, $(CXXFILES) $(TARGETS))

#
# These lines tell where to find a transactional STL.  We do our best to
# figure it out automatically, but assume there's a very new GCC in the path,
# and that it was used to build the STL
#
GCC_HOME    := $(shell which gcc | sed 's+/bin/gcc++')
TM_STL_HOME := ../../libs/libstdc++_tm
TM_STL_PATH := $(shell cd $(TM_STL_HOME); pwd)

#
# Use g++ in C++11 mode, TM enabled.
#
CXX      = g++
CXXFLAGS = -MMD -O3 -fgnu-tm -ggdb -m$(BITS) -std=c++11
LDFLAGS  = -m$(BITS) -litm -lrt

#
# Flags for transactional STL
#
CXXFLAGS += -nostdinc -nostdinc++
CXXFLAGS += -I${TM_STL_HOME}/libstdc++-v3/include
CXXFLAGS += -I${TM_STL_HOME}/libstdc++-v3/include/x86_64-unknown-linux-gnu
CXXFLAGS += -I${TM_STL_HOME}/libstdc++-v3/libsupc++
CXXFLAGS += -I${GCC_HOME}/include
CXXFLAGS += -I${GCC_HOME}/lib/gcc/x86_64-unknown-linux-gnu/6.0.0/include
CXXFLAGS += -I${GCC_HOME}/lib/gcc/x86_64-unknown-linux-gnu/6.0.0/include-fixed
CXXFLAGS += -I/usr/local/include/
CXXFLAGS += -I/usr/include
LDFLAGS  += -L$(TM_STL_PATH)/libstdc++-v3/src/obj$(BITS)
LDFLAGS  += -Wl,-rpath,$(TM_STL_PATH)/libstdc++-v3/src/obj$(BITS)
LDFLAGS  += -lstdc++ -pthread

#
# Target Info
#
.DEFAULT_GOAL = all
.PRECIOUS: $(OFILES) $(EXEOFILES)
.PHONY:    all clean

#
# Targets
#
all: $(EXEFILES)

clean:
	rm -rf $(ODIR)

#
# Rules for building .o files from sources
#
$(ODIR)/%.o: %.cc
	@echo "[CXX] $< --> $@"
	@$(CXX) $< -o $@ -c $(CXXFLAGS)

#
# Rules for building executable files... we'll be lazy and link all of the
# OFILES, even if we don't need them...
#
$(ODIR)/%: $(ODIR)/%.o $(OFILES)
	@echo "[LD] $< --> $@"
	@$(CXX) $^ -o $@ $(LDFLAGS) 

#
# Include dependencies
#
-include $(DEPS)
