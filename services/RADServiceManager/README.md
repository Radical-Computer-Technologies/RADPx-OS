# RADServiceManager

RADServiceManager is the planned first userspace process for RADPx-OS service
startup. The kernel currently exposes a small boot-service registry and overlay
configuration API so early trusted builds can start services deterministically.

The active PID 1 implementation now lives in this folder:

- `radinit.c`: JSON-backed service manager and service-control runtime.
- `radinit_entry.S`: minimal userspace entry shim for current freestanding
  x86_64 builds.

Target-specific build trees, such as `tools/embedded/x86_64_grub_slint`, should
consume this source and provide only packaging glue, generated rootfs content,
and target service JSON/templates. Service policy belongs here: priority,
restart behavior, dependency ordering, and user/session service ownership.
