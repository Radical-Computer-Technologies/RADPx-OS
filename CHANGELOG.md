# Changelog

All notable changes to RADPx-OS are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project aims to adhere to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.4-beta.2] - 2026-07-22

### Added

- **Multi-window RADCompositor desktop shell (x86 VM).** The Slint window manager
  now hosts three independently movable/resizable/closable app windows with exclusive
  focus and z-order: a **Terminal** (radsh over a PTY), a **File Explorer**, and a
  **Text Editor**.
- **File Explorer** with a real clickable listing (folder/file icons, hover, a path
  breadcrumb) that navigates directories via the in-kernel VFS.
- **Text Editor** window: a Slint `TextEdit` with an Open/Save toolbar backed by the
  kernel VFS.
- **Dynamic Slint UIs in the freestanding kernel.** `for`-Repeaters over `[struct]`
  models and Slint `std-widgets` (`Button`/`LineEdit`/`TextEdit`/`ScrollView`) now
  link and run under `-nostdlib -fno-exceptions` by providing the required libstdc++
  support symbols (container `__throw_*` handlers as reported kernel panics,
  `_Sp_make_shared_tag::_S_eq`, `__libc_single_threaded`).
- **Gradient dock** with open-order app icons (first-opened at the top), a per-icon
  right-click "Close" menu, and window geometry that resets to defaults on reopen.
- `vim` (vim-tiny) included in the x86 RADCompositor image.

### Changed

- 2026-07-18: Rebranded to **RADPx-OS** (Radical Posix OS) with the kernel named
  **RADKernel**. Renamed the code namespace `radix` -> `rad` and `RADixKernel`
  -> `RADKernel`. The public `rad_` / `RAD_` API prefixes are unchanged.
- Simplified the compositor to a provably-correct baseline (always copy-forward,
  full-footprint damage); `-enable-kvm` makes the removed micro-optimizations moot.

### Fixed

- Terminal no longer blanks to black (the kernel transcript feed no longer overwrites
  live radsh output) and no longer freezes on a command-not-found (execve pre-flights
  the target and returns -1 so the shell prints "command not found" and exits 127).
- Terminal keyboard input restored under the multi-window key routing (keys route to
  the focused window; only the terminal forwards to the PTY).
- Dragging a window over another no longer corrupts the overlapped window's rendering.

### Embedded

- a53/Pi brought to x86 interrupt/SMP-safety parity: Pi FP/NEON save across interrupt
  entry and an a53 page-allocator lock.
