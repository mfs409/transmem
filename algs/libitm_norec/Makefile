#
# Tools
#
CXX = g++
CC  = gcc

#
# Output Folders:  We build 32-bit and 64-bit versions of the SO
#
SO64DIR  = obj64
SO32DIR  = obj32
tmp1    := $(shell mkdir -p $(SO64DIR))
tmp3    := $(shell mkdir -p $(SO32DIR))

#
# Flags
#
IFLAGS     = -I./ -I./config/linux/x86 -I./config/linux -I./config/x86 -I./config/posix -I./config/generic
CFLAGS64   = -m64 -DHAVE_CONFIG_H $(IFLAGS) -mrtm -Wall -pthread -Werror -g -O2 -MMD
CFLAGS32   = -m32 -DHAVE_CONFIG_H $(IFLAGS) -march=i486 -mtune=generic -fomit-frame-pointer -mrtm -Wall -pthread -Werror -g -O2 -MMD
CXXFLAGS64 = -nostdinc++ $(CFLAGS64) -std=gnu++0x -funwind-tables -fno-exceptions -fno-rtti -fabi-version=4 -D_GNU_SOURCE
CXXFLAGS32 = -nostdinc++ $(CFLAGS32) -std=gnu++0x -funwind-tables -fno-exceptions -fno-rtti -fabi-version=4 -D_GNU_SOURCE
PICFLAGS   = -fPIC -DPIC

#
# Files
#
ASMFILES = sjlj
CXXFILES = aatree alloc alloc_c alloc_cpp barrier beginend clone eh_cpp local \
           query retry rwlock useraction util tls method-serial method-norec  \
           x86_sse x86_avx futex bst
O64FILES = $(patsubst %, $(SO64DIR)/%.o, $(ASMFILES) $(CXXFILES))
SO64NAME = $(SO64DIR)/libitm.so
O32FILES = $(patsubst %, $(SO32DIR)/%.o, $(ASMFILES) $(CXXFILES))
SO32NAME = $(SO32DIR)/libitm.so
DFILES   = $(patsubst %.o, %.d, $(O64FILES) $(O32FILES))

#
# Targets
#
all: $(SO64NAME) $(SO32NAME)
clean:
	@echo removing output folders
	@rm -rf $(SO64DIR) $(SO32DIR)
.PHONY: all clean

#
# 64-bit .so Build Rules
#
# NB: paths and avx/sse keep this from being super-simple
#
$(SO64NAME): $(O64FILES)
	@echo [LD] $@
	@$(CC) -m64 -shared $(PICFLAGS) $^ -mrtm -pthread -Wl,-O1 -Wl,--version-script -Wl,./libitm.map -Wl,-soname -Wl,libitm.so.1 -o $@
$(SO64DIR)/%.o: ./%.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS64) -c $< $(PICFLAGS) -o $@
$(SO64DIR)/%.o: ./config/linux/%.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS64) -c $< $(PICFLAGS) -o $@
$(SO64DIR)/%.o: ./config/x86/%.S
	@echo [CC] $@
	@$(CC) $(CFLAGS64) -c $< $(PICFLAGS) -o $@
$(SO64DIR)/%.o: ./config/generic/%.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS64) -c $< $(PICFLAGS) -o $@
$(SO64DIR)/x86_sse.o: ./config/x86/x86_sse.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS64) -msse -c $< $(PICFLAGS) -o $@
$(SO64DIR)/x86_avx.o: ./config/x86/x86_avx.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS64) -mavx -c $< $(PICFLAGS) -o $@

#
# 32-bit .so Build Rules
#
# NB: paths and avx/sse keep this from being super-simple
#
$(SO32NAME): $(O32FILES)
	@echo [LD] $@
	@$(CC) -m32 -shared $(PICFLAGS) $^ -march=i486 -mtune=generic -mrtm -pthread -Wl,-O1 -Wl,--version-script -Wl,./libitm.map -pthread -Wl,-soname -Wl,libitm.so.1 -o $@
$(SO32DIR)/%.o: ./%.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS32) -c $< $(PICFLAGS) -o $@
$(SO32DIR)/%.o: ./config/linux/%.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS32) -c $< $(PICFLAGS) -o $@
$(SO32DIR)/%.o: ./config/x86/%.S
	@echo [CC] $@
	@$(CC) $(CFLAGS32) -c $< $(PICFLAGS) -o $@
$(SO32DIR)/%.o: ./config/generic/%.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS32) -c $< $(PICFLAGS) -o $@
$(SO32DIR)/x86_sse.o: ./config/x86/x86_sse.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS32) -msse -c $< $(PICFLAGS) -o $@
$(SO32DIR)/x86_avx.o: ./config/x86/x86_avx.cc
	@echo [CXX] $@
	@$(CXX) $(CXXFLAGS32) -mavx -c $< $(PICFLAGS) -o $@

#
# Dependencies
#
-include $(DFILES)

