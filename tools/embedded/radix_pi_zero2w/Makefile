#
# Standalone RADix-OS payload for Raspberry Pi Zero 2 W.
#

RADIX_OS_ROOT ?= ../../..
RADLIB_ROOT ?= $(RADIX_OS_ROOT)/../RADLib
TOOLCHAIN_PREFIX ?= $(RADLIB_ROOT)/.radbuild/toolchains/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-

CXX := $(TOOLCHAIN_PREFIX)g++
CC := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy

CXXFLAGS := -std=c++20 -Os -ffreestanding -fno-pic -fno-pie -fno-rtti -fno-exceptions -fno-stack-protector -Wall -Wextra -DRADLIB_EMBEDDED_NO_CPP_WRAPPERS=1 -DRADIX_KERNEL_BACKEND_bcm283x_pi=1
CFLAGS := -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra -DRADLIB_EMBEDDED_NO_CPP_WRAPPERS=1 -DRADIX_KERNEL_BACKEND_bcm283x_pi=1
ASFLAGS := -x assembler-with-cpp
INCLUDES := -I$(RADIX_OS_ROOT)/RADixKernel/headers -I$(RADIX_OS_ROOT)/RADixKernel/boot
LDFLAGS := -nostdlib -static -Wl,-T,$(CURDIR)/linker.ld -Wl,--build-id=none

OBJS := \
	boot.o \
	kernel.o \
	aarch64_user.o \
	radboot.o \
	radixkernel_runtime.o \
	radixkernel_overlay.o \
	radixkernel_hal_bcm283x.o \
	radixkernel_runtime_newlib.o

all: RADIXKRN.ELF RADIXKRN.IMG

RADIXKRN.ELF: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) -Wl,--start-group -lc -lm -lgcc -Wl,--end-group

RADIXKRN.IMG: RADIXKRN.ELF
	$(OBJCOPY) -O binary $< $@

boot.o: boot.S
	$(CC) $(CFLAGS) $(ASFLAGS) -c -o $@ $<

kernel.o: kernel.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

aarch64_user.o: aarch64_user.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radboot.o: $(RADIX_OS_ROOT)/RADixKernel/boot/radboot.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radixkernel_runtime.o: $(RADIX_OS_ROOT)/RADixKernel/core/RADixKernel_runtime.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radixkernel_overlay.o: $(RADIX_OS_ROOT)/RADixKernel/subsystems/RADixOverlayRuntime.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radixkernel_hal_bcm283x.o: $(RADIX_OS_ROOT)/RADixKernel/hal/bcm283x/radixkernel_hal_bcm283x.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radixkernel_runtime_newlib.o: $(RADIX_OS_ROOT)/RADixKernel/hal/circle/radixkernel_runtime_newlib.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(OBJS) RADIXKRN.ELF RADIXKRN.IMG

.PHONY: all clean
