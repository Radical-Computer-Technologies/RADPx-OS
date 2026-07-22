# RADCompositor {#radcompositor}

RADCompositor is the Crimson 0.1.4 Slint-backed desktop service for RADPx-OS. It
is intentionally RADPx-OS-owned: Slint renders each UI surface, but RADPx-OS owns
the screen, window placement, z-order, focus, input routing, and the final
framebuffer writes.

As of 0.1.4-beta.2 the x86 VM target boots a real **multi-window desktop shell**
with a dock, an app launcher, and three kernel-rendered application windows —
Terminal, File Explorer, and Text Editor — each a live Slint surface.

## Architecture

Each window-like UI surface renders into its own off-screen pixel buffer. The
compositor (`RADCompositorCore`) tracks damaged rectangles, copies the current
front buffer forward into a software back buffer, redraws the intersecting
surfaces by z-order, and presents the touched rectangles to the framebuffer
backend through a double-buffered (front/back) present path. Surfaces may use
opaque XRGB pixels or ARGB pixels with source-over alpha blending. The
compositor holds up to 16 surfaces with a 64-entry damage ring.

The kernel-side runtime model (`EmbeddedDesktopShellModel` in `SlintShell.cpp`)
drives the shell: it owns each window record (bounds, z-order, focus, open
order), exclusive focus, and the dock/launcher state, and it maps the RADPx-OS
input queue onto the correct surface.

The x86 VM target starts these Slint surfaces:

- `RadDesktopSurface` — desktop background, top bar, and the gradient left dock
  (app launcher + open-app icons in open order, with a right-click "Close"
  dropdown per icon).
- `RadTerminalSurface` — a terminal window backed by a RADPx-OS PTY running the
  interactive shell.
- `RadFileExplorerSurface` — a real directory browser over the in-kernel VFS.
- `RadTextEditorSurface` — a text editor that opens and saves files through the
  VFS.

## Dynamic Slint in the freestanding kernel

The shell UI is **fully dynamic Slint**, not fixed-slot immediate mode. Slint
`for` repeaters over `[struct]` models and the `std-widgets` set (`Button`,
`LineEdit`, `TextEdit`, `ScrollView`) link and run in the freestanding
(`-nostdlib`, `-fno-exceptions`) kernel. This is enabled by a small set of
libstdc++ support symbols implemented in
`RADKernel/runtime/freestanding_runtime.cpp` (the `std::__throw_*` container
error handlers as diagnostic kernel panics, `std::_Sp_make_shared_tag::_S_eq`,
and `__libc_single_threaded = 1` for single-threaded shared_ptr refcounts). With
those, `std::make_shared<slint::VectorModel<T>>` works in the kernel exactly as
on the host.

Concretely:

- The File Explorer builds a `[RadFileEntry]` model from `rad_vfs_opendir` /
  `rad_vfs_readdir`, renders it with a repeater (folder/file icons, hover, a
  path breadcrumb), and descends directories via a `navigate(index)` callback.
- The Text Editor uses a `std-widgets` `TextEdit` for the document plus a
  `LineEdit` path field and Open/Save buttons, reading and writing through the
  VFS.
- The dock renders open-app icons from a `[RadDockIcon]` model in open order.

## Focus and input routing

Input events enter through the RADPx-OS input queue. Pointer coordinates are
tested against surface bounds in global screen space, translated into local
surface coordinates, and dispatched to the matching Slint window adapter; a
press on a window raises it and gives it exclusive focus.

Keyboard events route to the **focused** window's adapter. The Terminal role
translates keys to bytes and writes them to its PTY (so the interactive shell,
and escape sequences, reach the program); the other roles dispatch the key to
Slint so the Text Editor's `TextEdit` receives input. Escape no longer closes a
window — it dismisses any open menu/dropdown and otherwise forwards to the PTY.

## Shared-memory IPC and the client/server direction

The x86 VM target also exposes POSIX-inspired shared-memory syscalls for the
first process-boundary compositor path. A userspace producer can create a shm
object, `mmap` it, write pixels directly into that mapping, open
`/dev/compositor0`, attach the shm fd as a compositor surface, and queue dirty
rectangles with compositor ioctls. `/bin/radgfx-smoke` maps a one-page XRGB
surface and submits damage to prove that userspace pixels reach the compositor
without copying through a pipe or PTY.

The current app windows (Terminal, File Explorer, Text Editor) are trusted
in-process Slint surfaces. The next architectural step (the 0.1.5 line) is a
Wayland-style client/server model in which applications run as separate userland
processes that render into shared-memory surfaces composited by the kernel — the
shm producer path is the seed of that work.

## Current limits

The x86 GRUB framebuffer still uses software shadow buffers rather than a
hardware page flip, and surface buffers are currently allocated at full screen
resolution. Damage extraction is at the surface-rectangle granularity rather
than from Slint's internal dirty regions. The Pi (aarch64) target has parity
markers but does not yet link the Slint renderer; the ZuBoard is headless.
Future passes generalize shm-backed Slint applications (the client/server
model), size surface buffers per window rather than per screen, add SIMD
copy/blend backends, and make the cursor a compositor-managed surface.

## Verification

The hosted Slint self-test and the x86 VM smoke gate the compositor path with
markers for surface creation, off-screen rendering, dirty-queue submission,
copy-forward, dirty-framebuffer present, z-order, alpha blending, hit testing,
input coordinate translation, multi-window open/focus/close, and shared-memory
process IPC.
