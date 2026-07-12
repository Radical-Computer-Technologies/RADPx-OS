# Crimson 0.1.0 Status {#crimson_status}

Crimson 0.1.0 is an experimental beta line for RADix-OS. The branch goal is to
stabilize the kernel-facing API, x86 VM smoke path, Slint shell integration,
overlay binding, filesystem profile, and driver model enough for repeatable
testing.

## Stable-Beta Surface

- Kernel lifecycle, printk, boot info, time, memory stats, and terminal
  commands.
- Task, wait queue, mutex, event, timer, scheduler, and basic multicore
  reporting APIs.
- Device registry, ioctl encoding, input queues, framebuffer registration,
  TTY, PTY, block device, and VFS provider APIs.
- I2C, SPI, DMA, module, IRQ, and overlay binding APIs.
- x86 GRUB VM smoke target with framebuffer shell, PTY/TTY terminal path, and
  clean no-journal ext4 read/write profile.

## Experimental Surface

- POSIX process and syscall coverage is usable for the current shell tests but
  not a complete Unix userspace contract.
- Network APIs currently describe device/link/send/poll shape before full
  TCP/UDP stack work.
- RP2350 HSTX/DVI and SPI panel output are documented as framebuffer output
  goals, with backend completeness still target-dependent.
- DMA is available through the generic core and first consumed by SPI-style
  transfer paths.

## Not Yet Complete

- Full POSIX userland compatibility, dynamic ELF loading, complete fork/exec
  semantics across every target, full ext4 journaling, USB stacks, PCIe, and
  production networking are beyond the current Crimson beta surface.
