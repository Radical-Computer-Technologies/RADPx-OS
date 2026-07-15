# RADix-OS Kernel API

RADix-OS is a POSIX-inspired operating system kernel aimed at embedded boards,
desktop VM testing, and future SoC targets. The Crimson 0.2.0 package line keeps an
RTOS-influenced bias toward explicit subsystems while exposing familiar process,
file, device, terminal, framebuffer, filesystem, and driver APIs.

This hub documents the experimental Crimson 0.2.0 kernel/package API surface.

## Start Here

- @ref api_structure explains the public ABI shape and subsystem ownership.
- @ref api_groups provides a subsystem-oriented API index.
- @ref device_tree_guide documents the runtime overlay/device-tree model.
- @ref radcompositor describes the experimental Slint/RADCompositor windowing
  path.
- @ref networking describes the experimental IPv4/UDP and TCP stream socket
  foundation.
- @ref radbuild_integration describes the RadBuild 0.2.1 OS build and artifact
  publication flow.
- @ref pi_zero2w tracks the experimental two-stage Pi Zero 2 W bring-up.
- @ref minimal_examples provides short driver, framebuffer, input, VFS, and
  syscall examples.
- @ref crimson_status separates stable-beta APIs from incomplete areas.

## API Groups

- Kernel lifecycle, boot handoff, printk, CPU control, time, memory, and
  performance counters.
- Tasks, scheduler state, wait queues, mutexes, events, and timer sources.
- Processes, file descriptors, syscall dispatch, programs, and POSIX-inspired
  VFS, shared-memory, and memory-mapping entry points.
- Device registry, ioctl, block, network, framebuffer/display, input, TTY, PTY,
  audio, and serial APIs.
- Experimental IPv4/UDP datagram and local TCP stream socket support for the
  x86 VM path.
- Experimental Pi Zero 2 W `bcm283x_pi` payload and handoff ABI.
- Overlay tree, IRQ domains/resources, modules, I2C, SPI, and DMA driver APIs.

## Filesystem and Package Profile

The x86_64 VM target currently boots from ext4. Crimson 0.2.0 supports a clean
no-journal read/write ext4 profile with extent-backed files, allocation, create,
mkdir, rename, truncate, symlink/readlink, unlink, rmdir, chmod, and fsync.
Journal replay and journaled metadata commits are intentionally outside the
current beta profile.

RadBuild stages RADix packages through RadicalPackages `.radpm` packagegroups.
The standard terminal image selects `radix-terminal-base` and can add
`radix-networking`; RADCompositor images select `radix-desktop-base`.

## Verification

Host kernel tests:

```bash
cmake -S . -B build-host -DRADIX_OS_BUILD_TESTS=ON
cmake --build build-host -j2
./build-host/tests/RADixKernelTests
```

x86_64 GRUB + Slint VM smoke:

```bash
tools/embedded/x86_64_grub_slint_smoke.sh
```

Canonical RadBuild OS build:

```bash
radbuild build os --settings settings.json --json-events
```

Current experimental x86 bundles and `.radpm` packages are published through
the RadicalPackages GitHub Releases and package indexes.
