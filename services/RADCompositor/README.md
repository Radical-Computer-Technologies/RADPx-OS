# RADCompositor

RADCompositor is the experimental Crimson window-composition service. It keeps
Slint as a UI toolkit while RADPx owns the desktop coordinate space, input
routing, z-order, and final framebuffer blit.

The current implementation is monolithic and trusted: desktop and terminal
surfaces run in the boot UI process, render into off-screen ARGB/XRGB buffers,
and are then composed into the kernel framebuffer. This gives the x86 VM and
embedded framebuffer targets the same surface model before separate GUI
processes and IPC are introduced.

## Current Surface Model

- `RadDesktopSurface` renders the desktop, status line, and applications menu.
- `RadTerminalSurface` renders the first terminal application window.
- `RADCompositorCore` owns fixed surface slots, bounds, z-order, hit testing,
  alpha blending, and framebuffer composition.
- Input events are routed by global screen coordinates, translated to local
  surface coordinates, and dispatched to the matching Slint window adapter.

## Next Steps

- Replace the fixed x86 boot buffers with framebuffer-backed or allocator-backed
  surface storage.
- Move application windows to process-owned buffers shared with the compositor
  through a small IPC protocol.
- Add damage tracking so unchanged windows do not require full-frame software
  blits.
- Add cursor composition after the input stack has a stable pointer-device path.
