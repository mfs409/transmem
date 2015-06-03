#
# Let the user choose 32-bit or 64-bit compilation, but default to 32
#
BITS          ?= 32

#
# Directory Names
#
ODIR := ./obj$(BITS)
output_folder := $(shell mkdir -p $(ODIR))

#
# Location of GCC
#
GCC_HOME = $(shell which gcc | sed 's+/bin/gcc++')

#
# Names of files that the compiler generates
#
EXEFILES       = $(ODIR)/bench_tm $(ODIR)/bench_notm $(ODIR)/bench_trace
TM_OFILES      = $(patsubst %, $(ODIR)/%_tm.o, $(CXXFILES))
NOTM_OFILES    = $(patsubst %, $(ODIR)/%_notm.o, $(CXXFILES))
TRACE_OFILES   = $(patsubst %, $(ODIR)/%_trace.o, $(CXXFILES))
DEPS           = $(patsubst %.o, %.d, $(TM_OFILES) $(NOTM_OFILES) $(TRACE_OFILES))

#
# Paths to our libstdc++ source/build trees
#
TM_STL_HOME = ../../../libs/libstdc++_tm
NOTM_STL_HOME = ../../../libs/libstdc++_notm
TRACE_STL_HOME = ../../../libs/libstdc++_trace

#
# Tools
#
CXX = g++

#
# Flags
#
# NB: we use -MD instead of -MMD, because we want to see all the system
#     header dependencies... we're cloning the system headers and modifying
#     them, and want changes to force recompilation.
#
CXXFLAGS_TM    = -MD -O2 -fgnu-tm -ggdb -m$(BITS) -std=c++14 -nostdinc -nostdinc++ \
                 -I/usr/include/ -I$(TM_STL_HOME)/libstdc++-v3/include             \
                 -I$(TM_STL_HOME)/libstdc++-v3/include/x86_64-unknown-linux-gnu    \
                 -I$(TM_STL_HOME)/libstdc++-v3/libsupc++                           \
                 -I$(GCC_HOME)/lib/gcc/x86_64-unknown-linux-gnu/6.0.0/include      \
                 -DUSE_TM -pthread

CXXFLAGS_NOTM  = -MD -O2 -ggdb -m$(BITS) -std=c++14 -nostdinc -nostdinc++          \
                 -I/usr/include/ -I$(NOTM_STL_HOME)/libstdc++-v3/include           \
                 -I$(NOTM_STL_HOME)/libstdc++-v3/include/x86_64-unknown-linux-gnu  \
                 -I$(NOTM_STL_HOME)/libstdc++-v3/libsupc++                         \
                 -I$(GCC_HOME)/lib/gcc/x86_64-unknown-linux-gnu/6.0.0/include      \
                 -DNO_TM -pthread

CXXFLAGS_TRACE = -MD -O2 -ggdb -m$(BITS) -std=c++14 -nostdinc --nostdinc++         \
                 -include ../../libstdc++_trace/trace.h                            \
                 -I/usr/include/ -I$(TRACE_STL_HOME)/libstdc++-v3/include          \
                 -I$(TRACE_STL_HOME)/libstdc++-v3/include/x86_64-unknown-linux-gnu \
                 -I$(TRACE_STL_HOME)/libstdc++-v3/libsupc++                        \
                 -I$(GCC_HOME)/lib/gcc/x86_64-unknown-linux-gnu/6.0.0/include      \
                 -DNO_TM -pthread

LDFLAGS_NOTM   = -m$(BITS)                                                         \
                 -Wl,-rpath,$(NOTM_STL_HOME)/libstdc++-v3/src/obj$(BITS)/          \
                 -L$(NOTM_STL_HOME)/libstdc++-v3/src/obj$(BITS)/ -lstdc++ -pthread

LDFLAGS_TM     = -m$(BITS) -fgnu-tm                                                \
                 -Wl,-rpath,$(TM_STL_HOME)/libstdc++-v3/src/obj$(BITS)/            \
                 -L$(TM_STL_HOME)/libstdc++-v3/src/obj$(BITS)/ -lstdc++ -pthread

LDFLAGS_TRACE  = -m$(BITS)                                                         \
                 -Wl,-rpath,$(TRACE_STL_HOME)/libstdc++-v3/src/obj$(BITS)/         \
                 -L$(TRACE_STL_HOME)/libstdc++-v3/src/obj$(BITS)/ -lstdc++ -pthread

#
# Target Info
#
.DEFAULT_GOAL  = justtm
.PRECIOUS: $(TM_OFILES) $(NOTM_OFILES) $(TRACE_OFILES)
.PHONY: all clean justtm justnotm justtrace

#
# Targets
#
# NB: 'all' is *not* the default
#
justbase: $(ODIR)/bench_notm
justtrace: $(ODIR)/bench_trace
justtm: $(ODIR)/bench_tm
all: justtm justtrace justbase

#
# Rules for building .o files from sources
#
$(ODIR)/%_tm.o: %.cc
	@echo "[CXX] $< --> $@"
	@$(CXX) -c $< -o $@ $(CXXFLAGS_TM)

$(ODIR)/%_notm.o: %.cc
	@echo "[CXX] $< --> $@"
	@$(CXX) -c $< -o $@ $(CXXFLAGS_NOTM) $(LDFLAGS_NOTM)

$(ODIR)/%_trace.o: %.cc
	@echo "[CXX] $< --> $@"
	@$(CXX) -c $< -o $@ $(CXXFLAGS_TRACE)

#
# Rules for building executables
#

#
# The TM rules are broken right now, due to a compiler bug.  We should be
# able to build bench_tm like this:
#
#$(ODIR)/bench_tm: $(TM_OFILES)
#$(ODIR)/%_tm:$(ODIR)/%_tm.o
#	@echo "[LD] $^ --> $@"
#	@$(CXX) $^ -o $@ $(LDFLAGS_TM)
#
# However, when we do, all sorts of transaction-safe template instantiations
# that are invoked from multiple .o files result in 'multiple definition of
# ...' errors at link time.
#
# To work around the problem, for now, we use a hack.  We build everything at
# once, by concatenating all of the .cc files and then compiling it.  It's
# possible to work around this by carefully instantiating things in only one
# place, but it's not worth the effort since the fault is with GCC.
#
# Note, though, that for our code, this can result in some warnings for
# multiply-defined macros.
#
$(ODIR)/bench_tm: $(patsubst %, %.cc, $(CXXFILES))
	cat $(patsubst %, %.cc, $(CXXFILES)) > tmp.cc
	$(CXX) $(CXXFLAGS_TM) $(LDFLAGS_TM) -Wl,--verbose tmp.cc -o $@ 
	rm tmp.cc

#
# On the bright side, notm and trace builds work just fine...
#
$(ODIR)/bench_notm: $(NOTM_OFILES)
$(ODIR)/%_notm:$(ODIR)/%_notm.o
	@echo "[LD] $^ --> $@"
	$(CXX) $^ -o $@ $(LDFLAGS_NOTM)

$(ODIR)/bench_trace: $(TRACE_OFILES)
$(ODIR)/%_trace:$(ODIR)/%_trace.o
	@echo "[LD] $^ --> $@"
	@$(CXX) $^ -o $@ $(LDFLAGS_TRACE)

#
# To clean, we'll just clobber the build folder
#
clean:
	@echo Cleaning up...
	@rm -rf ./obj32 ./obj64

#
# Include dependencies
#
-include $(DEPS)
