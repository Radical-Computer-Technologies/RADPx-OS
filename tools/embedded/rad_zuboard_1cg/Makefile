#
# Standalone RADPx-OS serial payload for Avnet/Tria ZUBoard 1CG.
#

RADPX_OS_ROOT ?= ../../..
BOARD_DIR := $(RADPX_OS_ROOT)/RADKernel/boards/zuboard_1cg
RADLIB_ROOT ?= $(RADPX_OS_ROOT)/../RADLib
TOOLCHAIN_PREFIX ?= $(RADLIB_ROOT)/.radbuild/toolchains/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
RAD_ZYNQMP_SDHCI_BASE ?= 0xff170000u

CXX := $(TOOLCHAIN_PREFIX)g++
CC := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
AR := $(TOOLCHAIN_PREFIX)ar

RAD_ZUBOARD_DEFINES := -DRADLIB_EMBEDDED_NO_CPP_WRAPPERS=1 -DRAD_KERNEL_BACKEND_zynqmp_zuboard_1cg=1 -DRAD_ZYNQMP_SDHCI_BASE=$(RAD_ZYNQMP_SDHCI_BASE) -DRAD_KERNEL_MAX_PROCESSES=128u -DRAD_KERNEL_MAX_TASKS=128u -DRAD_KERNEL_TASK_STACK_BYTES=524288u -DRAD_A53_MAX_USER_PROCESSES=128u -DRAD_A53_MAX_TRACKED_PAGES=65536u -DRAD_A53_USABLE_FLOOR=0x08000000u -DRAD_KERNEL_ARCH_CONTEXT_WORDS=16u
# Userland may use FP/NEON: the exception paths in boot.S save/restore the full
# FP/SIMD state in the 816-byte trap frame. The KERNEL stays -mgeneral-regs-only
# so it never holds live FP state of its own.
A53_USER_ARCH_FLAGS := -mcpu=cortex-a53 -mbranch-protection=none
A53_KERNEL_ARCH_FLAGS := $(A53_USER_ARCH_FLAGS) -mgeneral-regs-only
CXXFLAGS := -std=c++20 -Os $(A53_KERNEL_ARCH_FLAGS) -ffreestanding -fno-pic -fno-pie -fno-rtti -fno-exceptions -fno-stack-protector -Wall -Wextra $(RAD_ZUBOARD_DEFINES)
CFLAGS := -Os $(A53_KERNEL_ARCH_FLAGS) -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra $(RAD_ZUBOARD_DEFINES)
ASFLAGS := -x assembler-with-cpp
USER_LDFLAGS := -nostdlib -static -fno-pic -fno-pie -Wl,-Ttext=0x80000000 -Wl,--build-id=none -Wl,-e,_start
INCLUDES := -I$(RADPX_OS_ROOT)/RADKernel/headers -I$(RADPX_OS_ROOT)/RADKernel/boot -I$(RADPX_OS_ROOT)/RADKernel/platforms/a53
USER_BUILD_DIR := userland-a53
USER_INCLUDES := -I$(RADPX_OS_ROOT)/libs/libradc/include -I$(RADPX_OS_ROOT)/userland/core/login -I$(USER_BUILD_DIR)
USER_CFLAGS := -Os $(A53_USER_ARCH_FLAGS) -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fomit-frame-pointer -Wall -Wextra $(USER_INCLUDES)
USER_ASFLAGS := $(ASFLAGS) $(USER_INCLUDES)
LDFLAGS := -nostdlib -static -Wl,-T,$(BOARD_DIR)/linker.ld -Wl,--build-id=none

OBJS := \
	boot.o \
	a53_freestanding.o \
	kernel.o \
	a53_init_image.o \
	a53_radsh_image.o \
	a53_sh_image.o \
	radkernel_a53.o \
	radboot.o \
	radkernel_runtime.o \
	radkernel_overlay.o \
	radkernel_ext4.o \
	radkernel_fat.o \
	radkernel_hal_zynqmp.o \
	gem_cadence.o \
	radkernel_runtime_newlib.o

USERLAND_ELFS := \
	$(USER_BUILD_DIR)/radinit.elf \
	$(USER_BUILD_DIR)/login.elf \
	$(USER_BUILD_DIR)/rash.elf

USERLAND_LIBS := \
	$(USER_BUILD_DIR)/libradc.a \
	$(USER_BUILD_DIR)/libradc.rso

all: rad-zuboard.elf rad-zuboard.img userland ports

userland: $(USERLAND_ELFS) $(USERLAND_LIBS)

# Upstream ports (ncurses + vim-tiny) cross-built with the shared x86 port
# scripts via their arch knobs. Stamp-cached: rebuilt only when the build
# scripts change (the scripts themselves wipe their build dirs on each run).
# The port scripts cd into their build directories, so every path handed to
# them must be absolute.
PORTS_OS_ROOT := $(abspath $(RADPX_OS_ROOT))
PORTS_DIR := $(USER_BUILD_DIR)/ports
PORTS_SCRIPTS_DIR := $(PORTS_OS_ROOT)/tools/embedded/x86_64_grub_slint/ports/upstream
PORTS_ENV := RAD_CC=$(abspath $(TOOLCHAIN_PREFIX)gcc) \
	RAD_PORT_STATIC_ARCH_FLAGS="-mcpu=cortex-a53 -mbranch-protection=none" \
	RAD_PORT_TTEXT=0x80000000 \
	RAD_PORT_HOST_TRIPLET=aarch64-none \
	RAD_PORT_CRT0_SOURCE=$(PORTS_OS_ROOT)/userland/runtime/aarch64_entry.S \
	RAD_BUILD_JOBS=$(RAD_PORTS_JOBS)
RAD_PORTS_JOBS ?= 4
PORTS_BUILD_ROOT := $(PORTS_OS_ROOT)/.radbuild/ports

ports: $(PORTS_DIR)/.stamp

$(PORTS_DIR)/.stamp: $(PORTS_SCRIPTS_DIR)/build_upstream_ncurses.sh $(PORTS_SCRIPTS_DIR)/build_upstream_vim_tiny.sh $(USER_BUILD_DIR)/libradc.a | $(USER_BUILD_DIR)
	mkdir -p $(PORTS_DIR)
	TIC_EXECUTABLE=$$(command -v tic) $(PORTS_ENV) \
		RAD_LIBC_ARCHIVE=$(CURDIR)/$(USER_BUILD_DIR)/libradc.a \
		$(PORTS_SCRIPTS_DIR)/build_upstream_ncurses.sh \
		$(PORTS_BUILD_ROOT)/src/ncurses-6.6 \
		$(PORTS_BUILD_ROOT)/build/ncurses-aarch64 \
		$(PORTS_BUILD_ROOT)/install/ncurses-aarch64 \
		$(PORTS_OS_ROOT)/libs/libradc
	$(PORTS_ENV) \
		RAD_SYSROOT_DIR=$(PORTS_OS_ROOT)/libs/libradc \
		RAD_NCURSES_INCLUDE_DIR=$(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/include \
		RAD_LIBC_ARCHIVE=$(CURDIR)/$(USER_BUILD_DIR)/libradc.a \
		RAD_NCURSESW_ARCHIVE=$(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/lib/libncursesw.a \
		RAD_TINFO_ARCHIVE=$(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/lib/libtinfo.a \
		$(PORTS_SCRIPTS_DIR)/build_upstream_vim_tiny.sh \
		$(PORTS_BUILD_ROOT)/src/vim \
		$(PORTS_BUILD_ROOT)/build/vim-aarch64
	cp $(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/lib/libncursesw.a $(PORTS_DIR)/libncursesw.a
	cp $(PORTS_BUILD_ROOT)/install/ncurses-aarch64/usr/lib/libtinfo.a $(PORTS_DIR)/libtinfo.a
	cp $(PORTS_BUILD_ROOT)/build/vim-aarch64/src/vim $(PORTS_DIR)/vim
	$(TOOLCHAIN_PREFIX)strip $(PORTS_DIR)/vim
	touch $@

rad-zuboard.elf: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) -Wl,--start-group -lc -lm -lgcc -Wl,--end-group

rad-zuboard.img: rad-zuboard.elf
	$(OBJCOPY) -O binary $< $@

boot.o: $(BOARD_DIR)/boot.S
	$(CC) $(CFLAGS) $(ASFLAGS) -c -o $@ $<

# a53 uses the toolchain newlib (-lc -lm) for malloc/snprintf plus mem.c for the
# mem/string family. The unified freestanding runtime (RADKernel/runtime/) is
# used by x86; switching a53 to it is deferred pending an intermittent heap-path
# corruption seen only on a53 (init load hangs; see the milestone memory).
a53_freestanding.o: $(RADPX_OS_ROOT)/libs/freestanding/mem.c
	$(CC) $(CFLAGS) -fno-builtin -c -o $@ $<

kernel.o: $(BOARD_DIR)/kernel.cpp
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

radkernel_hal_zynqmp.o: $(RADPX_OS_ROOT)/RADKernel/platforms/a53/zynqmp/radkernel_hal_zynqmp.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

gem_cadence.o: $(RADPX_OS_ROOT)/RADKernel/drivers/net/gem_cadence.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

radkernel_runtime_newlib.o: $(RADPX_OS_ROOT)/RADKernel/hal/circle/radkernel_runtime_newlib.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(USER_BUILD_DIR):
	mkdir -p $@

$(USER_BUILD_DIR)/rkconfig.h: | $(USER_BUILD_DIR)
	printf '%s\n' '#ifndef RAD_GENERATED_RKCONFIG_H' '#define RAD_GENERATED_RKCONFIG_H' '#define RAD_RKCONFIG_HOSTNAME "rad"' '#define RAD_RKCONFIG_ROOTFS_SIZE_MB 256' '#define RAD_RKCONFIG_TERMINAL_SCALE "auto"' '#define RAD_RKCONFIG_TERMINAL_FONT "rad-default"' '#define RAD_RKCONFIG_TERMINAL_THEME "rad-dark"' '#define RAD_RKCONFIG_TERMINAL_AUTOCOMPLETE 1' '#define RAD_RKCONFIG_TERMINAL_POSIX_COMPAT 1' '#define RAD_RKCONFIG_TERMINAL_NCURSES 1' '#define RAD_RKCONFIG_TERMINAL_NANO 0' '#define RAD_RKCONFIG_TERMINAL_NANO_VARIANT "none"' '#define RAD_RKCONFIG_AUTH_PASSWORDS 1' '#endif' > $@

$(USER_BUILD_DIR)/libradc.o: $(RADPX_OS_ROOT)/libs/libradc/rad_libc.c | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -fno-pic -fno-pie -nostdinc -c -o $@ $<

$(USER_BUILD_DIR)/libradc.pic.o: $(RADPX_OS_ROOT)/libs/libradc/rad_libc.c | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -fPIC -nostdinc -c -o $@ $<

$(USER_BUILD_DIR)/libradc.a: $(USER_BUILD_DIR)/libradc.o
	$(AR) crs $@ $<

$(USER_BUILD_DIR)/libradc.rso: $(USER_BUILD_DIR)/libradc.pic.o
	$(CC) -nostdlib -shared -Wl,--hash-style=sysv -Wl,--build-id=none -Wl,-soname,libradc.rso -o $@ $<

$(USER_BUILD_DIR)/radinit.elf: $(RADPX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADPX_OS_ROOT)/services/RADServiceManager/radinit.c $(USER_BUILD_DIR)/rkconfig.h $(USER_BUILD_DIR)/libradc.a | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -DRAD_USER_MAIN=radinit_main $(USER_LDFLAGS) -o $@ $(RADPX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADPX_OS_ROOT)/services/RADServiceManager/radinit.c $(USER_BUILD_DIR)/libradc.a

$(USER_BUILD_DIR)/login.elf: $(RADPX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADPX_OS_ROOT)/userland/core/login/login.c $(RADPX_OS_ROOT)/userland/core/login/auth.c $(USER_BUILD_DIR)/libradc.a | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -DRAD_USER_MAIN=login_main $(USER_LDFLAGS) -o $@ $(RADPX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADPX_OS_ROOT)/userland/core/login/login.c $(RADPX_OS_ROOT)/userland/core/login/auth.c $(USER_BUILD_DIR)/libradc.a

$(USER_BUILD_DIR)/rash.elf: $(RADPX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADPX_OS_ROOT)/userland/core/rash/radsh.c $(RADPX_OS_ROOT)/userland/core/login/auth.c $(USER_BUILD_DIR)/rkconfig.h $(USER_BUILD_DIR)/libradc.a | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -DRAD_USER_MAIN=radsh_main $(USER_LDFLAGS) -o $@ $(RADPX_OS_ROOT)/userland/runtime/aarch64_entry.S $(RADPX_OS_ROOT)/userland/core/rash/radsh.c $(RADPX_OS_ROOT)/userland/core/login/auth.c $(USER_BUILD_DIR)/libradc.a

clean:
	rm -f $(OBJS) a53-init.elf a53-radsh.elf a53-sh.elf rad-zuboard.elf rad-zuboard.img
	rm -rf $(USER_BUILD_DIR)

.PHONY: all clean userland
