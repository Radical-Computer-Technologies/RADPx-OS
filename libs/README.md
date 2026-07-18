# RADPx Libraries

This directory owns RADPx-provided userspace and kernel-adjacent library source.
Target build trees under `tools/` may package these libraries into a sysroot,
rootfs, archive, or `.rso`, but they should not own the library implementation.

Third-party libraries built from upstream source belong under `ports/` metadata
and RadBuild-managed source caches rather than this tree.
