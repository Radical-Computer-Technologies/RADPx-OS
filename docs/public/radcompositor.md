# RADCompositor {#radcompositor}

RADCompositor is an experimental Crimson 0.2.0 service for Slint-backed desktop
composition. It is intentionally RADLib/RADPx-owned: Slint renders each UI
surface, but RADPx owns the screen, window placement, z-order, input routing,
and final framebuffer writes.

## Architecture

Each window-like UI surface renders into an off-screen pixel buffer. The
compositor tracks damaged rectangles, copies the current front buffer forward
into a software back buffer for those rectangles, redraws intersecting surfaces
by z-order, and presents only the touched rectangles to the framebuffer backend.
Surfaces may use opaque XRGB pixels or ARGB pixels with alpha blending.

The x86 VM target currently starts two Slint surfaces:

- `RadDesktopSurface` for the desktop background, status, and applications menu.
- `RadTerminalSurface` for the first terminal window backed by a RADPx PTY.

Input events enter through the RADPx input queue. Pointer coordinates are tested
against compositor surface bounds in global screen space, translated into local
surface coordinates, and then dispatched to the matching Slint window adapter.

## Shared-Memory IPC

The x86 VM target exposes POSIX-inspired shared-memory syscalls for the first
process-boundary compositor path. A userspace producer can create a shm object,
map it with `mmap`, write pixels directly into that mapping, open
`/dev/compositor0`, attach the shm fd as a compositor surface, and queue dirty
rectangles with compositor ioctls.

This first producer path is intentionally small: `/bin/radgfx-smoke` maps a
one-page XRGB surface and submits damage to prove that userspace pixels reach
the compositor without copying through a pipe or PTY.

## Current Limits

This path is still experimental. The boot desktop and terminal surfaces are
trusted in-process objects, while the smoke producer is a separate process using
shared pages. The x86 GRUB framebuffer still uses software shadow buffers rather
than hardware page flip. Future passes should generalize shm-backed Slint
applications, add damage extraction from Slint internals, add SIMD copy/blend
backends, and make the cursor a compositor-managed surface.

## Verification

The hosted Slint example and x86 VM smoke gate the compositor path with markers
for surface creation, off-screen rendering, dirty queue submission, copy-forward,
dirty framebuffer present, z-order, alpha blending, hit testing, input
coordinate translation, and shared-memory process IPC.
