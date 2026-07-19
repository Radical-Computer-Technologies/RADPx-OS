#
# Standalone RADPx-OS payload for Raspberry Pi Zero 2 W.
#

RADPX_OS_ROOT ?= ../../..
RADLIB_ROOT ?= $(RADPX_OS_ROOT)/../RADLib
TOOLCHAIN_PREFIX ?= $(RADLIB_ROOT)/.radbuild/toolchains/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-

CXX := $(TOOLCHAIN_PREFIX)g++
CC := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy

# Kernel sizing (mirrors the ZuBoard). RAD_KERNEL_TASK_STACK_BYTES is critical:
# load_user_program puts a ~64 KB rad_a53_address_space_t on the stack, which
# overflows the default 8 KB task stack on the execve-reenter path (the second
# program load runs inside the user-process task, not the large boot stack).
# RAD_KERNEL_ARCH_CONTEXT_WORDS must be 16 to match the a53 context-switch asm.
RAD_PI_DEFINES := -DRADLIB_EMBEDDED_NO_CPP_WRAPPERS=1 -DRAD_KERNEL_BACKEND_bcm283x_pi=1 \
	-DRAD_KERNEL_MAX_PROCESSES=128u -DRAD_KERNEL_MAX_TASKS=128u -DRAD_KERNEL_TASK_STACK_BYTES=524288u \
	-DRAD_A53_MAX_USER_PROCESSES=128u -DRAD_A53_MAX_TRACKED_PAGES=65536u -DRAD_KERNEL_ARCH_CONTEXT_WORDS=16u

CXXFLAGS := -std=c++20 -Os -ffreestanding -fno-pic -fno-pie -fno-rtti -fno-exceptions -fno-stack-protector -Wall -Wextra $(RAD_PI_DEFINES)
CFLAGS := -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra $(RAD_PI_DEFINES)
ASFLAGS := -x assembler-with-cpp
USER_LDFLAGS := -nostdlib -static -fno-pic -fno-pie -Wl,-Ttext=0x80000000 -Wl,--build-id=none -Wl,-e,_start
INCLUDES := -I$(RADPX_OS_ROOT)/RADKernel/headers -I$(RADPX_OS_ROOT)/RADKernel/boot -I$(RADPX_OS_ROOT)/RADKernel/platforms/a53
LDFLAGS := -nostdlib -static -Wl,-T,$(CURDIR)/linker.ld -Wl,--build-id=none

OBJS := \
	boot.o \
	kernel.o \
	a53_init_image.o \
	a53_radsh_image.o \
	a53_sh_image.o \
	radkernel_a53.o \
	a53_parity_selftest.o \
	radboot.o \
	radkernel_runtime.o \
	radkernel_overlay.o \
	radkernel_ext4.o \
	radkernel_fat.o \
	radkernel_hal_bcm283x.o \
	radkernel_runtime_newlib.o

all: RADKRN.ELF RADKRN.IMG

RADKRN.ELF: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) -Wl,--start-group -lc -lm -lgcc -Wl,--end-group

RADKRN.IMG: RADKRN.ELF
	$(OBJCOPY) -O binary $< $@

boot.o: boot.S
	$(CC) $(CFLAGS) $(ASFLAGS) -c -o $@ $<

kernel.o: kernel.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

a53-init.elf: $(RADPX_OS_ROOT)/userland/embedded/a53/init.S
	$(CC) $(CFLAGS) $(ASFLAGS) $(USER_LDFLAGS) -o $@ $<

a53-radsh.elf: $(RADPX_OS_ROOT)/userland/embedded/a53/radsh.S
	$(CC) $(CFLAGS) $(ASFLAGS) $(USER_LDFLAGS) -o $@ $<

a53-sh.elf: $(RADPX_OS_ROOT)/userland/embedded/a53/sh.S
	$(CC) $(CFLAGS) $(ASFLAGS) $(USER_LDFLAGS) -o $@ $<

a53_init_image.o: a53-init.elf
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

a53_radsh_image.o: a53-radsh.elf
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

a53_sh_image.o: a53-sh.elf
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

radkernel_a53.o: $(RADPX_OS_ROOT)/RADKernel/platforms/a53/radkernel_a53.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

a53_parity_selftest.o: $(RADPX_OS_ROOT)/RADKernel/platforms/a53/a53_parity_selftest.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radboot.o: $(RADPX_OS_ROOT)/RADKernel/boot/radboot.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radkernel_runtime.o: $(RADPX_OS_ROOT)/RADKernel/core/RADKernel_runtime.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radkernel_overlay.o: $(RADPX_OS_ROOT)/RADKernel/subsystems/RADOverlayRuntime.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radkernel_ext4.o: $(RADPX_OS_ROOT)/RADKernel/fs/ext4.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(RADPX_OS_ROOT)/RADKernel/fs -c -o $@ $<

radkernel_fat.o: $(RADPX_OS_ROOT)/RADKernel/fs/fat.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(RADPX_OS_ROOT)/RADKernel/fs -c -o $@ $<

radkernel_hal_bcm283x.o: $(RADPX_OS_ROOT)/RADKernel/platforms/a53/bcm283x/radkernel_hal_bcm283x.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radkernel_runtime_newlib.o: $(RADPX_OS_ROOT)/RADKernel/hal/circle/radkernel_runtime_newlib.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(OBJS) a53-init.elf a53-radsh.elf a53-sh.elf RADKRN.ELF RADKRN.IMG

.PHONY: all clean
