# API Structure {#api_structure}

RADPx-OS exposes one stable C ABI through `radixkernel.h`. The smaller
subsystem headers, such as `rad_vfs.h` and `rad_spi.h`, include that ABI and
exist so drivers and applications can include the subsystem they use.

## Initialization And Boot

Platform code fills `rad_boot_info_t` when firmware or a bootloader has already
provided memory maps, command-line arguments, an initrd pointer, or an FDT
pointer. The kernel is then initialized with `rad_kernel_init()` and driven by
`rad_kernel_poll()` or `rad_kernel_run()`.

Use `rad_printk()` for normal kernel logging and `rad_early_printk()` before the
regular console path is registered.

## Runtime Services

The core runtime is intentionally explicit:

- Tasks, wait queues, timers, mutexes, and events provide RTOS-style in-kernel
  concurrency primitives.
- Process, syscall, file descriptor, and VFS APIs provide the POSIX-inspired
  userspace path.
- Device, block, network, framebuffer, input, TTY, PTY, serial, audio, I2C,
  SPI, DMA, IRQ, and module APIs are the driver-facing kernel boundary.
- The overlay tree binds platform description to buses, child devices,
  framebuffers, and interrupt domains.

## Platform Layout

Architecture and SoC code is split under `RADixKernel/platforms`. Portable A53
execution code belongs in `platforms/a53`; Broadcom-specific Pi drivers belong
under `platforms/a53/bcm283x`. The x86 VM target remains the parity reference
and will move under `platforms/x86` once its target wrapper is thin enough to
avoid behavior changes.

## Handle Rules

Opaque handles are owned by the caller after a successful open/create call.
Close or destroy them with the matching close/destroy function. Functions return
`RAD_STATUS_OK` on success, a negative `rad_status_t` value on kernel API
errors, or a negative POSIX-style result for file descriptor/syscall wrappers.

## Crimson Stability

Crimson 0.2.0 documents the intended public shape for beta work. APIs marked as
experimental may change before the stable release; APIs marked stable-beta
should keep source compatibility inside the Crimson branch unless a documented
release note says otherwise.
