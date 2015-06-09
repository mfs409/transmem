#
# Let the user choose 32-bit or 64-bit compilation, but default to 32
#
BITS ?= 32

#
# Tools
#
CXX = g++

#
# Flags
#
CXXFLAGS = -ggdb -fgnu-tm -std=c++11 -m$(BITS) -I../../libs/libtmcondvar -O3 -MMD
LDFLAGS	 = -m$(BITS) -lrt -L../../libs/libtmcondvar/obj$(BITS)/ -ltmcondvar -fgnu-tm

#
# Directory Names
#
ODIR          := ./obj$(BITS)
output_folder := $(shell mkdir -p $(ODIR))

#
# Files to compile that don't have a main() function
#
CXXFILES = 

#
# Files to compile that do have  amain() function
#
TARGETS=bounded-buffer

#
# Names of files that the compiler generates
#
EXEFILES  = $(patsubst %, $(ODIR)/%,   $(TARGETS))
OFILES    = $(patsubst %, $(ODIR)/%.o, $(CXXFILES))
EXEOFILES = $(patsubst %, $(ODIR)/%.o, $(TARGETS))
DEPS      = $(patsubst %, $(ODIR)/%.d, $(CXXFILES) $(TARGETS))

#
# Target Info
#
.PHONY: all clean

all: $(EXEFILES)

clean:
	@echo Cleaning up...
	@rm -rf $(ODIR)

#
# Rules for building object files
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
