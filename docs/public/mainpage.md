# RADix-OS Kernel API

RADix-OS is a POSIX-inspired operating system kernel aimed at embedded boards,
desktop VM testing, and future SoC targets. The Crimson 0.1.0 line keeps an
RTOS-influenced bias toward explicit subsystems while exposing familiar process,
file, device, terminal, framebuffer, filesystem, and driver APIs.

This hub documents the experimental Crimson 0.1.0 kernel API.

## Start Here

- @ref api_structure explains the public ABI shape and subsystem ownership.
- @ref api_groups provides a subsystem-oriented API index.
- @ref device_tree_guide documents the runtime overlay/device-tree model.
- @ref minimal_examples provides short driver, framebuffer, input, VFS, and
  syscall examples.
- @ref crimson_status separates stable-beta APIs from incomplete areas.

## API Groups

- Kernel lifecycle, boot handoff, printk, CPU control, time, memory, and
  performance counters.
- Tasks, scheduler state, wait queues, mutexes, events, and timer sources.
- Processes, file descriptors, syscall dispatch, programs, and POSIX-inspired
  VFS entry points.
- Device registry, ioctl, block, network, framebuffer/display, input, TTY, PTY,
  audio, and serial APIs.
- Overlay tree, IRQ domains/resources, modules, I2C, SPI, and DMA driver APIs.

## Filesystem Profile

The x86_64 VM target currently boots from ext4. Crimson 0.1.0 supports a clean
no-journal read/write ext4 profile with extent-backed files, allocation, create,
mkdir, rename, truncate, symlink/readlink, unlink, rmdir, chmod, and fsync.
Journal replay and journaled metadata commits are intentionally outside the
current beta profile.

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

The generated ISO is a development artifact and is not published in
RadicalPackages.
