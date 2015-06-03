#
# Set up tools
#
CXX=gcc
AR=ar
RANLIB=ranlib

#
# Set up build folders
#
ODIR64 := ./obj64
ODIR32 := ./obj32
o64_folder := $(shell mkdir -p $(ODIR64))
o32_folder := $(shell mkdir -p $(ODIR32))

#
# Set up filenames... we track based on what folder they are in
# (../../libiberty or ./ (../libsupc++)) and whether they use C++98, C++11,
# or C++14.  We also build two identical libraries, for compatibility with
# gcc
#
NAMES_IBERTY = cp-demangle
NAMES_SUP98  = array_type_info atexit_arm bad_alloc bad_cast bad_typeid	class_type_info del_op del_opnt del_opv del_opvnt dyncast eh_alloc eh_arm eh_call eh_catch eh_exception eh_globals eh_personality eh_term_handler eh_tm eh_type eh_unex_handler enum_type_info function_type_info fundamental_type_info guard_error hash_bytes new_opv new_opvnt pbase_type_info pmem_type_info pointer_type_info pure si_class_type_info tinfo tinfo2 vec vmi_class_type_info vterminate
NAMES_SUP11  = atexit_thread bad_array_length bad_array_new eh_aux_runtime eh_ptr eh_terminate eh_throw guard nested_exception new_handler new_op new_opnt
NAMES_SUP98_TM = 
NAMES_SUP14  = del_ops del_opvs
NAMES_LIB    = libsupc++convenience libsupc++

#
# Set up common flags for libsupc++ and libiberty files.  Note that 32-bit
# adds -m32 twice, for some reason.  We're copying it identically from the
# build, even though it's redundant
#
SUPFLAGS64 = -shared-libgcc -nostdinc++ -I../../libgcc -I../include/x86_64-unknown-linux-gnu -I../include -I./ -D_GLIBCXX_SHARED -fno-implicit-templates -Wall -Wextra -Wwrite-strings -Wcast-qual -Wabi -fdiagnostics-show-location=once -ffunction-sections -fdata-sections -g -O2 -D_GNU_SOURCE
SUPFLAGS32 = -shared-libgcc -nostdinc++ -m32 -I../../libgcc -I../include/x86_64-unknown-linux-gnu -I../include -I./ -D_GLIBCXX_SHARED -fno-implicit-templates -Wall -Wextra -Wwrite-strings -Wcast-qual -Wabi -fdiagnostics-show-location=once -ffunction-sections -fdata-sections -g -O2 -D_GNU_SOURCE -m32
IBERTYFLAGS64 = -DHAVE_CONFIG_H -I../ -I../../libiberty -I../../include -D_GLIBCXX_SHARED -I../include/x86_64-unknown-linux-gnu -I../include -I./ -g -O2 -DIN_GLIBCPP_V3 -Wno-error
IBERTYFLAGS32 = -m32 -DHAVE_CONFIG_H -I../ -I../../libiberty -I../../include -D_GLIBCXX_SHARED -I../include/x86_64-unknown-linux-gnu -I../include -I./ -g -O2 -m32 -DIN_GLIBCPP_V3 -Wno-error
PICFLAGS = -fPIC -DPIC

#
# Name manipulations to produce .o and .a target names (64-bit and 32-bit)
#
O64SUP98 = $(patsubst %, $(ODIR64)/%.o, $(NAMES_SUP98))
O64SUP11 = $(patsubst %, $(ODIR64)/%.o, $(NAMES_SUP11))
O64SUP14 = $(patsubst %, $(ODIR64)/%.o, $(NAMES_SUP14))
O64IBERTY = $(patsubst %, $(ODIR64)/%.o, $(NAMES_IBERTY))
A64 = $(patsubst %, $(ODIR64)/%.a, $(NAMES_LIB))

O32SUP98 = $(patsubst %, $(ODIR32)/%.o, $(NAMES_SUP98))
O32SUP11 = $(patsubst %, $(ODIR32)/%.o, $(NAMES_SUP11))
O32SUP14 = $(patsubst %, $(ODIR32)/%.o, $(NAMES_SUP14))
O32IBERTY = $(patsubst %, $(ODIR32)/%.o, $(NAMES_IBERTY))
A32 = $(patsubst %, $(ODIR32)/%.a, $(NAMES_LIB))


#
# Target info
#
.DEFAULT_GOAL = all
.PHONY: all clean

#
# Targets
#
all: $(A64) $(A32)

clean:
	rm -rf $(ODIR32) $(ODIR64)

#
# Build rules
#
$(O64SUP98): $(ODIR64)/%.o: %.cc
	$(CXX) $(SUPFLAGS64) -frandom-seed=$(basename $(notdir $@)).lo -c $<  $(PICFLAGS) -D_GLIBCXX_SHARED -o $@

$(O64SUP11): $(ODIR64)/%.o: %.cc
	$(CXX) $(SUPFLAGS64) -std=gnu++11 -frandom-seed=$(basename $(notdir $@)).lo -c $<  $(PICFLAGS) -D_GLIBCXX_SHARED -o $@

$(O64SUP14): $(ODIR64)/%.o: %.cc
	$(CXX) -fgnu-tm $(SUPFLAGS64) -std=gnu++14 -Wno-sized-deallocation -frandom-seed=$(basename $(notdir $@)).lo -c $<  $(PICFLAGS) -D_GLIBCXX_SHARED -o $@

$(O64IBERTY): $(ODIR64)/%.o: ../../libiberty/%.c
	$(CXX) $(IBERTYFLAGS64) -c $<  $(PICFLAGS) -o $@

$(ODIR64)/%.a: $(O64SUP98) $(O64SUP11) $(O64SUP14) $(O64IBERTY)
	$(AR) rc $@ $^
	$(RANLIB) $@

$(O32SUP98): $(ODIR32)/%.o: %.cc
	$(CXX) $(SUPFLAGS32) -frandom-seed=$(basename $(notdir $@)).lo -c $<  $(PICFLAGS) -D_GLIBCXX_SHARED -o $@

$(O32SUP11): $(ODIR32)/%.o: %.cc
	$(CXX) $(SUPFLAGS32) -std=gnu++11 -frandom-seed=$(basename $(notdir $@)).lo -c $<  $(PICFLAGS) -D_GLIBCXX_SHARED -o $@

$(O32SUP14): $(ODIR32)/%.o: %.cc
	$(CXX) -fgnu-tm $(SUPFLAGS32) -std=gnu++14 -Wno-sized-deallocation -frandom-seed=$(basename $(notdir $@)).lo -c $<  $(PICFLAGS) -D_GLIBCXX_SHARED -o $@

$(O32IBERTY): $(ODIR32)/%.o: ../../libiberty/%.c
	$(CXX) $(IBERTYFLAGS32) -c $<  $(PICFLAGS) -o $@

$(ODIR32)/%.a: $(O32SUP98) $(O32SUP11) $(O32SUP14) $(O32IBERTY)
	$(AR) rc $@ $^
	$(RANLIB) $@

