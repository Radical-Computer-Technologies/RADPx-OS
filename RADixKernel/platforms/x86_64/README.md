# RADPx x86_64 Platform

This directory owns x86_64-specific kernel/platform implementation for the GRUB
boot path: early boot assembly, linker script, CPU/IRQ/VM glue, userspace entry,
and current x86-backed storage/filesystem adapters.

The `tools/embedded/x86_64_grub_slint` directory consumes this source to build
bootable test images. It should contain image packaging and target configuration
only, not platform source ownership.
