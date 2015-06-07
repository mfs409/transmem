#
# NB: This is an easy library to build, so we build the 32-bit and 64-bit .a
# files as part of 'all'
#

#
# Basic C++11 + TM build configuration
#
CXX	  = g++
CXXFLAGS  = -MMD -ggdb -O3 -fgnu-tm -std=c++11 -pthread
LDFLAGS   = -lrt

#
# Uncomment this line to enable debug (statistics counting)
#
# CXXFLAGS += -DDEBUG

#
# Output directory names
#
ODIR32          := ./obj32
ODIR64          := ./obj64
output_folder32 := $(shell mkdir -p $(ODIR32))
output_folder64 := $(shell mkdir -p $(ODIR64))

#
# For now, libtmcondvar only requires condvar.cpp
#
CXXFILES   = condvar
LIBFILES32 = $(ODIR32)/libtmcondvar.a
LIBFILES64 = $(ODIR64)/libtmcondvar.a

#
# Generate .o and .d names from the source files
#
OFILES32 = $(patsubst %, $(ODIR32)/%.o, $(CXXFILES))
OFILES64 = $(patsubst %, $(ODIR64)/%.o, $(CXXFILES))
DEPS32   = $(patsubst %, $(ODIR32)/%.d, $(CXXFILES))
DEPS64   = $(patsubst %, $(ODIR64)/%.d, $(CXXFILES))

#
# Target info
#
.PHONY: all

#
# Targets
#
all: $(LIBFILES32) $(LIBFILES64)

clean:
	@echo cleaning up...
	@rm -rf $(ODIR32) $(ODIR64)

#
# Build rules
#
$(ODIR32)/%.o:%.cpp
	@echo [CXX] $< "-->" $@
	@$(CXX) -m32 $(CXXFLAGS) -c -o $@ $<

$(ODIR64)/%.o:%.cpp
	@echo [CXX] $< "-->" $@
	@$(CXX) -m64 $(CXXFLAGS) -c -o $@ $<

#
# Archive rules
#
$(LIBFILES32): $(OFILES32)
	@echo [AR] $^ "-->" $@
	@$(AR) cru $@ $^

$(LIBFILES64): $(OFILES64)
	@echo [AR] $^ "-->" $@
	@$(AR) cru $@ $^

#
# Import dependency info
#
-include $(DEPS32) $(DEPS64)
