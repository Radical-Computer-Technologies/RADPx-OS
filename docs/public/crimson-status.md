# Crimson 0.1.4 Status {#crimson_status}

Crimson 0.1.4 is an experimental beta package line for RADPx-OS. The branch goal is to
stabilize the kernel-facing API, x86 VM smoke path, Slint shell integration,
overlay binding, filesystem profile, and driver model enough for repeatable
testing.

## Stable-Beta Surface

- Kernel lifecycle, printk, boot info, time, memory stats, and terminal
  commands.
- Task, wait queue, mutex, event, timer, scheduler, and basic multicore
  reporting APIs.
- Device registry, ioctl encoding, input queues, framebuffer registration,
  TTY, PTY, block device, USB host-info, and VFS provider APIs.
- I2C, SPI, DMA, module, IRQ, and overlay binding APIs.
- x86 GRUB VM smoke target with `rash` as the default shell, framebuffer
  terminal, PTY/TTY path, JSON `radinit` services, persistent logs, FAT32 test
  volume, clean no-journal ext4 read/write profile, and RadBuild 0.2.1
  `os.rad` build orchestration.
- RadicalPackages `.radpm` packagegroups for terminal, desktop, networking,
  and SDK image composition.

## Experimental Surface

- POSIX process and syscall coverage is usable for the current shell tests but
  not a complete Unix userspace contract.
- `/bin/sh` is currently pointed at `rash` for the x86 image so shebang scripts
  exercise the same shell implementation as the interactive path.
- Shared-memory `mmap`/`shm_open` support is present for the x86 compositor
  smoke path, but not yet a complete POSIX memory-mapping implementation.
- RADCompositor now boots a multi-window desktop shell on the x86 VM target: a
  gradient dock with an app launcher and open-order icons (right-click to close),
  and three kernel-rendered Slint application windows — Terminal (real PTY),
  File Explorer (VFS directory browser), and Text Editor (VFS open/save). Windows
  move, resize, focus/raise, and close, with per-window keyboard routing
  (Terminal keys go to the PTY, Editor keys go to its Slint `TextEdit`).
- The shell UI is fully dynamic Slint in the freestanding kernel: `for` repeaters
  over `[struct]` models and the `std-widgets` set (`Button`, `LineEdit`,
  `TextEdit`, `ScrollView`) link and run under `-nostdlib`/`-fno-exceptions` via
  a small set of libstdc++ support symbols in the kernel runtime.
- RADCompositor also exposes dirty-rectangle software composition and an
  shm-backed producer surface path. Full Slint *userspace* apps (a Wayland-style
  client/server model where apps are separate processes rendering into shared
  memory), per-window surface allocation, and hardware page flip are the 0.1.5
  follow-up.
- Network APIs now include device/link/send/receive/poll shape, experimental
  IPv4/UDP datagram sockets, local TCP stream socket lifecycle support, and
  legacy virtio-net RX/TX queue ownership markers for the x86 VM target.
- RP2350 HSTX/DVI and SPI panel output are documented as framebuffer output
  goals, with backend completeness still target-dependent.
- Pi Zero 2 W has an experimental standalone `bcm283x_pi` payload, handoff ABI,
  platform split under `platforms/a53`, PL011 serial path, timer reads, mailbox
  framebuffer path, SD block-device registration scaffold, USB/input scaffolds,
  A53 MMU/table setup, process/COW self-tests, and Slint/RADCompositor parity
  markers.
- DMA is available through the generic core and first consumed by SPI-style
  transfer paths.

## Not Yet Complete

- Full POSIX userland compatibility, dynamic ELF loading, complete fork/exec
  semantics across every target, full ext4 journaling, USB stacks, PCIe,
  wire-level TCP, DHCP/DNS, and production networking are beyond the current
  Crimson beta surface.
- The Pi Zero 2 W path still needs hardware-real Circle FAT loader jump
  validation, eMMC command implementation, DWC OTG USB host enumeration,
  hardware HID input, full AArch64 EL0 ELF execution/fork/exec parity, and Slint
  framebuffer rendering before it matches the x86 VM path.
