# RADLib Embedded Setup

These scripts set up local embedded build environments without installing
toolchains globally. RADixKernel currently has smoke paths for RP235x Pico
SDK targets and Circle Pi Zero 2 W targets.

## Install the Pi Zero 2 W 64-bit toolchain

```bash
tools/embedded/setup_circle_toolchains.sh --arch aarch64
```

The toolchain is extracted under `.radbuild/toolchains/`.

For RP235x Pico SDK builds, install the Cortex-M toolchain too:

```bash
tools/embedded/setup_circle_toolchains.sh --arch arm32
```

## Use the environment in the current shell

```bash
source tools/embedded/circle_env.sh
```

This only updates the current shell process. It does not edit `.bashrc`,
`.profile`, or system paths.

## Build Circle for Pi Zero 2 W

```bash
tools/embedded/build_circle.sh --target pi-zero-2w-64 --clean
```

Pi Zero 2 W and Pi 3 share Circle's 64-bit `RASPPI=3` configuration. For the
FPiGA Audio Hat family, build the unique 64-bit configs with:

```bash
tools/embedded/build_circle.sh --target audiohat-64 --clean
```

That builds:

- `pi-zero-2w-64` / `pi3-64` using Circle `RASPPI=3`
- `pi4-64` using Circle `RASPPI=4`

Use `--realtime` when validating lower-latency interrupt behavior for audio or
control paths. Use `--c++20` only when an application needs C++20 features.

## Build the standalone RADPx Pi Zero 2 W payload

The new Pi bring-up path separates Circle from the RADPx runtime. Raspberry Pi
firmware remains the real first-stage boot path; Circle is being narrowed into
an optional second-stage loader for kernel selection, maintenance, reflash, and
validated handoff. The RADPx-owned payload is built as `RADIXKRN.IMG` and does
not link Circle:

```bash
make -C tools/embedded/radix_pi_zero2w
tools/embedded/radix_pi_zero2w_smoke.sh
```

This payload currently brings up the RAD-owned BCM283x backend skeleton:
PL011 serial, system timer reads, mailbox framebuffer registration,
`/dev/mmcblk0` registration with QEMU smoke sector reads, `/dev/usb0` host-info
registration, `/dev/input/event0` synthetic HID events, AArch64 process parity
markers, and Slint/RADCompositor parity markers. The real BCM283x eMMC command
engine, DWC OTG USB host enumeration, hardware HID input, AArch64 page-table
implementation, and Slint rendering on Pi hardware are the next Pi-specific
driver work.

## Build the standalone RADPx ZUBoard 1CG serial payload

The ZuBoard path targets the Avnet/Tria ZUBoard 1CG Zynq UltraScale+ MPSoC
board. The first payload is intentionally serial-only and expects FSBL, PMU
firmware, and U-Boot to initialize clocks, DDR, and MIO before U-Boot enters
RADPx with `bootelf`.

```bash
make -C tools/embedded/radix_zuboard_1cg
```

The target emits:

- `tools/embedded/radix_zuboard_1cg/radix-zuboard.elf`
- `tools/embedded/radix_zuboard_1cg/radix-zuboard.img`

RadBuild template `radix-os-zuboard-serial` stages those files with a boot
script, the reference XSA from `../ZUBoard-1CG_RT` when available, generated
XSCT firmware scripts, a Bootgen BIF, a FAT boot partition image, an ext4 rootfs
image, and a combined SD image. The current kernel backend is
`zynqmp_zuboard_1cg` and registers the PS UART console, ZynqMP SDHCI block
device, MBR partitions, and an ext4 rootfs login path.

For QEMU smoke testing, build the QEMU profile and run the helper script:

```bash
radbuild build os --system radix-zuboard-1cg-qemu
RADIX_QEMU_BUILD=0 RADIX_QEMU_TIMEOUT=45 \
  tools/embedded/radix_zuboard_1cg/run-qemu.sh
```

The script uses QEMU's `xlnx-zcu102` machine model, passes the QEMU SDHCI base
to the build, pads the SD card image to 512 MiB, and starts only core 0. The
boot assembly parks nonzero A53 cores with `wfe`; secondary-core startup should
stay disabled until the ZynqMP GIC and per-core scheduler path are implemented.

## Build the Slint source cache

Embedded Slint builds should reuse one persistent source checkout instead of
cloning into every CMake build directory:

```bash
tools/embedded/fetch_slint_source.sh
```

The checkout defaults to `.radbuild/slint-src` and tracks `release/1.17`.
Override with `SLINT_SOURCE_DIR=/path/to/slint` or `SLINT_REF=<ref>`.
Building Slint from source requires a recent Rust toolchain; Slint 1.17's C++
API currently requires Rust 1.92 or newer. RAD embedded source builds set
Slint's Rust static library to `panic=abort`. On hosted Rust targets, the
prebuilt stable sysroot may still contain unwind personality references; the
freestanding x86 runtime provides abort-only personality/unwind stubs for that
case.

## Build the RAD OS Slint shell smoke target

The shell target validates the RAD framebuffer contract and Slint software
renderer without requiring display hardware:

```bash
cmake -S tools/embedded/rad_os_shell -B build/embedded/rad_os_shell \
  -DRADUI_SLINT_PROVIDER=binary \
  -DSLINT_SDK_DIR=/media/jvincent/Kingspec512/SDKs/Slint-cpp-1.17.0-Linux-x86_64
cmake --build build/embedded/rad_os_shell --target rad_os_shell -j2
./build/embedded/rad_os_shell/rad_os_shell --self-test
```

The target registers `/dev/fb0` as the primary RAD framebuffer and renders a
RADUi-owned desktop shell through Slint. The shell has an Applications icon, an
Applications menu with a single Terminal entry, Escape-driven menu/window
closing, and a PTY-backed terminal window. Slint is the renderer/input surface;
RADUi owns the shell state and menu/window behavior.

## Build the x86 GRUB handoff target

The x86 target currently builds a Multiboot2 GRUB handoff kernel ELF and can
package an ISO when GRUB and mtools are installed:

```bash
cmake -S tools/embedded/x86_grub -B build/embedded/x86_grub
cmake --build build/embedded/x86_grub --target radixkernel_x86_grub -j2
cmake --build build/embedded/x86_grub --target radixkernel_x86_grub_iso -j2
```

ISO packaging requires `grub-mkrescue`, `xorriso`, and `mformat` from mtools.

## Build and run the x86_64 GRUB RADKernel VM target

The x86_64 target boots through BIOS/GRUB Multiboot2, registers the GRUB
framebuffer as `/dev/fb0`, registers the emulated PS/2 keyboard as
`/dev/input/event0`, registers the polled PS/2 mouse as `/dev/input/event1`,
opens a RADKernel PTY-backed boot terminal, and renders the RADLib/Slint
desktop shell directly into the kernel framebuffer. The shell starts at an
Applications icon/menu, launches the Terminal entry as a managed window with
title-bar focus, close, drag, and resize callbacks, and displays the PTY
transcript from the boot terminal. The smoke script also runs the hosted Slint
framebuffer shell first when the Slint SDK is available, then injects a
QEMU virtual keyboard command and pointer movement through the monitor when
`socat` is installed:

```bash
tools/embedded/x86_64_grub_slint_smoke.sh
```

For an interactive VM window in a desktop session:

```bash
tools/embedded/run_x86_64_grub_slint_vm.sh
```

Use `--headless` for serial-only verification or `--display sdl` if GTK display
support is unavailable. In Virt-Manager, use legacy BIOS boot with the normal
emulated keyboard; QEMU will usually expose host USB keyboards to the guest as
the legacy i8042/PS/2 keyboard this target currently supports. The ISO now links
Slint's Rust/C++ runtime as a static freestanding software renderer and verifies
`RADIX_SLINT_BOOT_SHELL_OK`, `RADIX_SLINT_TERMINAL_LOADING_OK`, and
`RADIX_SLINT_TERMINAL_READY_OK`, `RADIX_SLINT_WM_OK`, and
`RADIX_SLINT_APP_TERMINAL_WINDOW_OK` during the VM smoke.

The first RADPx POSIX substrate is also present in this target. It initializes
pid 1, integer file descriptors with `0/1/2` bound to `/dev/console`, installs a
GDT/TSS/IDT, provides an `int 0x80` syscall entry path, inventories multiboot
memory into a simple physical page allocator, and boots a tiny freestanding
`/bin/init` from a generated ext2 rootfs attached as legacy virtio-blk. The
freestanding x86 runtime now runs C++ global constructors, provides no-exception
`new`/`delete` hooks including aligned and nothrow forms, registers trusted
kernel modules through the RAD module lifecycle API, and uses explicit
copy-in/copy-out buffers for the user-pointer syscall paths exercised by the VM
smoke. User init now runs from a dedicated x86_64 page table with
supervisor-only kernel identity mappings, user-mapped ELF segments, a
user-mapped stack, and a guard page below that stack.

The VM smoke builds `radix-rootfs.ext2`, attaches it as `/dev/vda`, mounts it at
`/`, loads `/bin/init`, enters CPL3, and checks `RADIX_X86_CPU_OK`,
`RADIX_CPP_RUNTIME_OK`, `RADIX_MODULE_LIFECYCLE_OK`, `RADIX_INT80_OK`,
`RADIX_POSIX_ABI_OK`, `RADIX_VM_READY`, `RADIX_VM_PAGE_ALLOC_OK`,
`RADIX_USER_VM_ISOLATED_OK`, `RADIX_USER_ENTRY_MAP_OK`, `RADIX_USER_COPY_OK`,
`RADIX_USER_SYSCALLS_OK`, `RADIX_USER_INVALID_PTR_OK`,
`RADIX_INPUT_POINTER_OK`, `RADIX_INPUT_POINTER_EVENT_OK`,
`RADIX_VIRTIO_BLK_READ_OK`, `RADIX_EXT2_MOUNT_OK`, `RADIX_ROOTFS_INIT_FOUND`,
`RADIX_USERMODE_ENTER_OK`, `RADIX_USER_INIT_OK`, `RADIX_USERMODE_EXIT_OK`,
`RADIX_SLINT_BOOT_SHELL_OK`, `RADIX_SLINT_TERMINAL_LOADING_OK`, and
`RADIX_SLINT_TERMINAL_READY_OK`. The remaining pieces for a fuller POSIX
userland are broader syscall coverage, page-fault driven COW, fork
address-space cloning, APIC support, interrupts for virtio queues, writes to
block devices, and a libc-backed init/shell.

## Notes

Circle is expected at `../circle` relative to the RADLib repo by default. Override
with:

```bash
RADLIB_CIRCLE_ROOT=/path/to/circle tools/embedded/build_circle.sh
```

The default target uses Circle `RASPPI=3` with the `aarch64-none-elf-` toolchain,
which is the Circle 64-bit configuration for Raspberry Pi 3-class SoCs including
Pi Zero 2 W.

## Build RADixKernel Smoke Targets

The preferred entrypoint is RADLib's Python/JSON build harness:

```bash
./build.py --build-embedded --embedded-target rp235x
./build.py --build-embedded --embedded-target circle
./build.py --build-embedded --embedded-target all
```

Embedded defaults live in `buildconfig.json` under `embedded`. CLI flags override
JSON for one build:

```bash
./build.py --build-embedded --embedded-target rp235x \
  --pico-sdk-path /media/jvincent/Kingspec512/SDKs/pico-sdk \
  --pico-board pico2 \
  --pico-platform rp2350-arm-s \
  --sd-mode auto
```

RP235x storage is SD-card only. SPI SD is the baseline. PIO SDIO is an optional
integration point:

```bash
./build.py --build-embedded --embedded-target rp235x --enable-sdio-pio --sd-mode pio_sdio
```

PIO SDIO requires board wiring with SD CLK, CMD, and four consecutive DAT GPIOs.
The kernel HAL is ready for a FatFs/PIO SDIO driver such as
`inindev/pico-fatfs-sd`, but RADLib does not vendor that third-party driver by
default.
