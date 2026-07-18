#
# Standalone RADix-OS serial payload for Avnet/Tria ZUBoard 1CG.
#

RADIX_OS_ROOT ?= ../../..
BOARD_DIR := $(RADIX_OS_ROOT)/RADixKernel/boards/zuboard_1cg
RADLIB_ROOT ?= $(RADIX_OS_ROOT)/../RADLib
TOOLCHAIN_PREFIX ?= $(RADLIB_ROOT)/.radbuild/toolchains/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
RADIX_ZYNQMP_SDHCI_BASE ?= 0xff170000u

CXX := $(TOOLCHAIN_PREFIX)g++
CC := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
AR := $(TOOLCHAIN_PREFIX)ar

RADIX_ZUBOARD_DEFINES := -DRADLIB_EMBEDDED_NO_CPP_WRAPPERS=1 -DRADIX_KERNEL_BACKEND_zynqmp_zuboard_1cg=1 -DRADIX_ZYNQMP_SDHCI_BASE=$(RADIX_ZYNQMP_SDHCI_BASE) -DRADIX_KERNEL_MAX_PROCESSES=128u -DRADIX_KERNEL_MAX_TASKS=128u -DRADIX_KERNEL_TASK_STACK_BYTES=524288u -DRADIX_A53_MAX_USER_PROCESSES=128u -DRADIX_A53_MAX_TRACKED_PAGES=65536u -DRADIX_A53_USABLE_FLOOR=0x08000000u -DRADIX_KERNEL_ARCH_CONTEXT_WORDS=16u
# Userland may use FP/NEON: the exception paths in boot.S save/restore the full
# FP/SIMD state in the 816-byte trap frame. The KERNEL stays -mgeneral-regs-only
# so it never holds live FP state of its own.
A53_USER_ARCH_FLAGS := -mcpu=cortex-a53 -mbranch-protection=none
A53_KERNEL_ARCH_FLAGS := $(A53_USER_ARCH_FLAGS) -mgeneral-regs-only
CXXFLAGS := -std=c++20 -Os $(A53_KERNEL_ARCH_FLAGS) -ffreestanding -fno-pic -fno-pie -fno-rtti -fno-exceptions -fno-stack-protector -Wall -Wextra $(RADIX_ZUBOARD_DEFINES)
CFLAGS := -Os $(A53_KERNEL_ARCH_FLAGS) -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra $(RADIX_ZUBOARD_DEFINES)
ASFLAGS := -x assembler-with-cpp
USER_LDFLAGS := -nostdlib -static -fno-pic -fno-pie -Wl,-Ttext=0x80000000 -Wl,--build-id=none -Wl,-e,_start
INCLUDES := -I$(RADIX_OS_ROOT)/RADixKernel/headers -I$(RADIX_OS_ROOT)/RADixKernel/boot -I$(RADIX_OS_ROOT)/RADixKernel/platforms/a53
USER_BUILD_DIR := userland-a53
USER_INCLUDES := -I$(RADIX_OS_ROOT)/libs/libradixc/include -I$(RADIX_OS_ROOT)/userland/core/login -I$(USER_BUILD_DIR)
USER_CFLAGS := -Os $(A53_USER_ARCH_FLAGS) -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fomit-frame-pointer -Wall -Wextra $(USER_INCLUDES)
USER_ASFLAGS := $(ASFLAGS) $(USER_INCLUDES)
LDFLAGS := -nostdlib -static -Wl,-T,$(BOARD_DIR)/linker.ld -Wl,--build-id=none

OBJS := \
	boot.o \
	freestanding_runtime.o \
	kernel.o \
	a53_init_image.o \
	a53_radsh_image.o \
	a53_sh_image.o \
	radixkernel_a53.o \
	radboot.o \
	radixkernel_runtime.o \
	radixkernel_overlay.o \
	radixkernel_ext4.o \
	radixkernel_hal_zynqmp.o \
	gem_cadence.o

USERLAND_ELFS := \
	$(USER_BUILD_DIR)/radinit.elf \
	$(USER_BUILD_DIR)/login.elf \
	$(USER_BUILD_DIR)/rash.elf

USERLAND_LIBS := \
	$(USER_BUILD_DIR)/libradixc.a \
	$(USER_BUILD_DIR)/libradixc.rso

all: radix-zuboard.elf radix-zuboard.img userland ports

userland: $(USERLAND_ELFS) $(USERLAND_LIBS)

# Upstream ports (ncurses + vim-tiny) cross-built with the shared x86 port
# scripts via their arch knobs. Stamp-cached: rebuilt only when the build
# scripts change (the scripts themselves wipe their build dirs on each run).
# The port scripts cd into their build directories, so every path handed to
# them must be absolute.
PORTS_OS_ROOT := $(abspath $(RADIX_OS_ROOT))
PORTS_DIR := $(USER_BUILD_DIR)/ports
PORTS_SCRIPTS_DIR := $(PORTS_OS_ROOT)/tools/embedded/x86_64_grub_slint/ports/upstream
PORTS_ENV := RADIX_CC=$(abspath $(TOOLCHAIN_PREFIX)gcc) \
	RADIX_PORT_STATIC_ARCH_FLAGS="-mcpu=cortex-a53 -mbranch-protection=none" \
	RADIX_PORT_TTEXT=0x80000000 \
	RADIX_PORT_HOST_TRIPLET=aarch64-none \
	RADIX_PORT_CRT0_SOURCE=$(PORTS_OS_ROOT)/userland/runtime/aarch64_entry.S \
	RADIX_BUILD_JOBS=$(RADIX_PORTS_JOBS)
RADIX_PORTS_JOBS ?= 4
PORTS_BUILD_ROOT := $(PORTS_OS_ROOT)/.radbuild/ports

ports: $(PORTS_DIR)/.stamp

$(PORTS_DIR)/.stamp: $(PORTS_SCRIPTS_DIR)/build_upstream_ncurses.sh $(PORTS_SCRIPTS_DIR)/build_upstream_vim_tiny.sh $(USER_BUILD_DIR)/libradixc.a | $(USER_BUILD_DIR)
	mkdir -p $(PORTS_DIR)
	TIC_EXECUTABLE=$$(command -v tic) $(PORTS_ENV) \
		RADIX_LIBC_ARCHIVE=$(CURDIR)/$(USER_BUILD_DIR)/libradixc.a \
		$(PORTS_SCRIPTS_DIR)/build_upstream_ncurses.sh \
		$(PORTS_BUILD_ROOT)/src/ncurses-6.6 \
		$(PORTS_BUILD_ROOT)/build/ncurses-aarch64 \
		$(PORTS_BUILD_ROOT)/install/ncurses-aarch64 \
		$(PORTS_OS_ROOT)/libs/libradixc
	$(PORTS_ENV) \
		RADIX_SYSROOT_DIR=$(PORTS_OS_ROOT)/libs/libradixc \
		RADIX_NCURSES_INCLUDE_DIR=$(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/include \
		RADIX_LIBC_ARCHIVE=$(CURDIR)/$(USER_BUILD_DIR)/libradixc.a \
		RADIX_NCURSESW_ARCHIVE=$(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/lib/libncursesw.a \
		RADIX_TINFO_ARCHIVE=$(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/lib/libtinfo.a \
		$(PORTS_SCRIPTS_DIR)/build_upstream_vim_tiny.sh \
		$(PORTS_BUILD_ROOT)/src/vim \
		$(PORTS_BUILD_ROOT)/build/vim-aarch64
	cp $(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/lib/libncursesw.a $(PORTS_DIR)/libncursesw.a
	cp $(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/lib/libtinfo.a $(PORTS_DIR)/libtinfo.a
	cp $(PORTS_BUILD_ROOT)/build/vim-aarch64/src/vim $(PORTS_DIR)/vim
	$(TOOLCHAIN_PREFIX)strip $(PORTS_DIR)/vim
	touch $@

radix-zuboard.elf: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) -Wl,--start-group -lgcc -Wl,--end-group

radix-zuboard.img: radix-zuboard.elf
	$(OBJCOPY) -O binary $< $@

boot.o: $(BOARD_DIR)/boot.S
	$(CC) $(CFLAGS) $(ASFLAGS) -c -o $@ $<

# Unified freestanding runtime (heap/malloc, mem/string, snprintf, C++ ABI stubs)
# shared with x86 -- replaces both libs/freestanding/mem.c and the newlib glue, so
# the a53 kernel no longer links the toolchain newlib (-lc -lm).
freestanding_runtime.o: $(RADIX_OS_ROOT)/RADixKernel/runtime/freestanding_runtime.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -fno-builtin -c -o $@ $<

kernel.o: $(BOARD_DIR)/kernel.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

a53-init.elf: $(RADIX_OS_ROOT)/userland/embedded/a53/init.S
	$(CC) $(CFLAGS) $(ASFLAGS) $(USER_LDFLAGS) -o $@ $<

a53-radsh.elf: $(RADIX_OS_ROOT)/userland/embedded/a53/radsh.S
	$(CC) $(CFLAGS) $(ASFLAGS) $(USER_LDFLAGS) -o $@ $<

a53-sh.elf: $(RADIX_OS_ROOT)/userland/embedded/a53/sh.S
	$(CC) $(CFLAGS) $(ASFLAGS) $(USER_LDFLAGS) -o $@ $<

a53_init_image.o: a53-init.elf
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

a53_radsh_image.o: a53-radsh.elf
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

a53_sh_image.o: a53-sh.elf
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

radixkernel_a53.o: $(RADIX_OS_ROOT)/RADixKernel/platforms/a53/radixkernel_a53.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radboot.o: $(RADIX_OS_ROOT)/RADixKernel/boot/radboot.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radixkernel_runtime.o: $(RADIX_OS_ROOT)/RADixKernel/core/RADixKernel_runtime.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radixkernel_overlay.o: $(RADIX_OS_ROOT)/RADixKernel/subsystems/RADixOverlayRuntime.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radixkernel_ext4.o: $(RADIX_OS_ROOT)/RADixKernel/fs/ext4.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(RADIX_OS_ROOT)/RADixKernel/fs -c -o $@ $<

radixkernel_hal_zynqmp.o: $(RADIX_OS_ROOT)/RADixKernel/platforms/a53/zynqmp/radixkernel_hal_zynqmp.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

gem_cadence.o: $(RADIX_OS_ROOT)/RADixKernel/drivers/net/gem_cadence.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radixkernel_runtime_newlib.o: $(RADIX_OS_ROOT)/RADixKernel/hal/circle/radixkernel_runtime_newlib.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(USER_BUILD_DIR):
	mkdir -p $@

$(USER_BUILD_DIR)/rkconfig.h: | $(USER_BUILD_DIR)
	printf '%s\n' '#ifndef RADIX_GENERATED_RKCONFIG_H' '#define RADIX_GENERATED_RKCONFIG_H' '#define RADIX_RKCONFIG_HOSTNAME "radix"' '#define RADIX_RKCONFIG_ROOTFS_SIZE_MB 256' '#define RADIX_RKCONFIG_TERMINAL_SCALE "auto"' '#define RADIX_RKCONFIG_TERMINAL_FONT "radix-default"' '#define RADIX_RKCONFIG_TERMINAL_THEME "radix-dark"' '#define RADIX_RKCONFIG_TERMINAL_AUTOCOMPLETE 1' '#define RADIX_RKCONFIG_TERMINAL_POSIX_COMPAT 1' '#define RADIX_RKCONFIG_TERMINAL_NCURSES 1' '#define RADIX_RKCONFIG_TERMINAL_NANO 0' '#define RADIX_RKCONFIG_TERMINAL_NANO_VARIANT "none"' '#define RADIX_RKCONFIG_AUTH_PASSWORDS 1' '#endif' > $@

$(USER_BUILD_DIR)/libradixc.o: $(RADIX_OS_ROOT)/libs/libradixc/radix_libc.c | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -fno-pic -fno-pie -nostdinc -c -o $@ $<

$(USER_BUILD_DIR)/libradixc.pic.o: $(RADIX_OS_ROOT)/libs/libradixc/radix_libc.c | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -fPIC -nostdinc -c -o $@ $<

$(USER_BUILD_DIR)/libradixc.a: $(USER_BUILD_DIR)/libradixc.o
	$(AR) crs $@ $<

$(USER_BUILD_DIR)/libradixc.rso: $(USER_BUILD_DIR)/libradixc.pic.o
	$(CC) -nostdlib -shared -Wl,--hash-style=sysv -Wl,--build-id=none -Wl,-soname,libradixc.rso -o $@ $<

$(USER_BUILD_DIR)/radinit.elf: $(RADIX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADIX_OS_ROOT)/services/RADServiceManager/radinit.c $(USER_BUILD_DIR)/rkconfig.h $(USER_BUILD_DIR)/libradixc.a | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -DRADIX_USER_MAIN=radinit_main $(USER_LDFLAGS) -o $@ $(RADIX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADIX_OS_ROOT)/services/RADServiceManager/radinit.c $(USER_BUILD_DIR)/libradixc.a

$(USER_BUILD_DIR)/login.elf: $(RADIX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADIX_OS_ROOT)/userland/core/login/login.c $(RADIX_OS_ROOT)/userland/core/login/auth.c $(USER_BUILD_DIR)/libradixc.a | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -DRADIX_USER_MAIN=login_main $(USER_LDFLAGS) -o $@ $(RADIX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADIX_OS_ROOT)/userland/core/login/login.c $(RADIX_OS_ROOT)/userland/core/login/auth.c $(USER_BUILD_DIR)/libradixc.a

$(USER_BUILD_DIR)/rash.elf: $(RADIX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADIX_OS_ROOT)/userland/core/rash/radsh.c $(RADIX_OS_ROOT)/userland/core/login/auth.c $(USER_BUILD_DIR)/rkconfig.h $(USER_BUILD_DIR)/libradixc.a | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -DRADIX_USER_MAIN=radsh_main $(USER_LDFLAGS) -o $@ $(RADIX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADIX_OS_ROOT)/userland/core/rash/radsh.c $(RADIX_OS_ROOT)/userland/core/login/auth.c $(USER_BUILD_DIR)/libradixc.a

clean:
	rm -f $(OBJS) a53-init.elf a53-radsh.elf a53-sh.elf radix-zuboard.elf radix-zuboard.img
	rm -rf $(USER_BUILD_DIR)

.PHONY: all clean userland
