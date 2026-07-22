# RADPx-OS (Radical Posix OS)

RADPx-OS is a POSIX-inspired operating system targeting hybrid RTOS-like
performance with a POSIX-like feature set. Its kernel is **RADKernel**.

The x86_64 GRUB target can boot either a framebuffer terminal or a **Slint
multi-window desktop shell** (RADCompositor): a gradient dock with an app
launcher and three kernel-rendered windows — Terminal (real PTY), File Explorer
(VFS browser), and Text Editor (VFS open/save) — driven by dynamic Slint (`for`
repeaters + `std-widgets`) running in the freestanding kernel. The Cortex-A53
ZuBoard-1CG and Pi Zero 2 W targets are earlier in bring-up (serial/marker and
two-stage payload respectively).

## Layout

- `RADKernel/` contains the portable kernel core, public
  `<radkernel/...>` headers, host simulator backend, and embedded/platform
  backends (x86_64, and the Cortex-A53 `a53/` tree with the ZynqMP/ZuBoard
  SoC backend).
- `tools/embedded/` contains the current bootable targets and smoke scripts:
  the x86_64 GRUB terminal and RADCompositor/Slint VM profiles, and the
  ZuBoard-1CG (ZynqMP A53) serial target under `rad_zuboard_1cg/`.
- `tests/` contains host-side RADPx-OS kernel tests.

RADLib is now an optional sibling dependency for RADLib/Slint UI integration.
Kernel-only builds do not require RADLib.

## Building with RadBuild

RadBuild is the build driver for RADPx-OS. Install the frozen release once and
invoke the `radbuild` command directly — no Python checkout or interpreter
handling required:

```bash
# From the published RadBuild release assets (Debian/apt-based hosts):
sudo apt install ./radbuild_0.2.1_amd64.deb
```

This places `radbuild` on `PATH`. Each `settings*.json` at the repo root is an
OS build configuration; RadBuild resolves it, builds the image, and runs the
target's smoke:

```bash
radbuild build os --settings settings.json          --json-events   # default x86_64 terminal
radbuild build os --settings settings.terminal.json --json-events   # x86_64 GRUB terminal
radbuild build os --settings settings.wm.json       --json-events   # x86_64 RADCompositor/Slint WM
radbuild build os --settings settings.ci.json --system rad-zuboard-1cg-qemu-ci --json-events  # ZuBoard-1CG A53 (QEMU)
```

RadBuild 0.2.1 treats each JSON as an OS build configuration. The terminal
profile excludes Slint/RADCompositor chunks and runs the terminal VM smoke; the
WM profile includes the Slint-backed RADCompositor desktop shell (dock,
launcher, Terminal/File Explorer/Text Editor windows), runs the hosted UI smoke,
and packages the VM image. The Slint VM path is not yet a release gate. The ZuBoard profile cross-builds the A53 kernel + userland,
assembles the SD image, and runs the QEMU marker smoke.
Each profile has its own build directory, artifact directory, writable ext4/FAT
images, serial logs, and `SHA256SUMS`.

The profile ISOs are generated at:

```text
build/embedded/x86_64_grub_terminal/radkernel-x86-64-grub-terminal.iso
build/embedded/x86_64_grub_wm/radkernel-x86-64-grub-wm.iso
```

> Development against an unreleased RadBuild can still run the source tree
> directly (`python3 ../RadBuild/radbuild/.tools/radbuild.py ...`), but the
> frozen `radbuild` command above is the supported entry point.

## Build Checks

Host kernel tests:

```bash
cmake -S . -B build-host -DRADPX_OS_BUILD_TESTS=ON
cmake --build build-host -j2
./build-host/tests/RADKernelTests
```

x86_64 GRUB terminal ISO smoke:

```bash
RAD_X86_UI_PROFILE=terminal tools/embedded/x86_64_grub_slint_smoke.sh
```

x86_64 GRUB RADCompositor/Slint ISO smoke:

```bash
RAD_X86_UI_PROFILE=wm tools/embedded/x86_64_grub_slint_smoke.sh
```

ZuBoard-1CG (ZynqMP A53) QEMU smokes — the marker gate boots to an interactive
serial login, the login smoke drives that session, and the SMP smoke validates
the second core under `-smp 2`:

```bash
tools/embedded/rad_zuboard_1cg/qemu_marker_smoke.sh   # ordered marker gate
tools/embedded/rad_zuboard_1cg/qemu_login_smoke.sh    # interactive login/ls/cat/ps
tools/embedded/rad_zuboard_1cg/qemu_smp_smoke.sh      # second A53 core (-smp 2)
```

## API Documentation

RADPx-OS uses Doxygen for the public Crimson 0.1.4 experimental kernel API
docs:

```bash
RAD_SOURCE_DIR="$PWD" RAD_DOCS_OUTPUT="$PWD/build/docs" doxygen docs/Doxyfile
```

The public Crimson docs are published through RadicalPackages under
`docs/radpx-os/0.1.4/api/`.

## Filesystems

The x86_64 and ZuBoard A53 root filesystems default to ext4. The current ext4
driver is a clean no-journal profile with extent-backed read/write, create,
mkdir, rename, truncate, symlink/readlink, unlink, rmdir, chmod, and fsync
support, exercised on both targets. Journal replay and journaled metadata
commits are intentionally deferred.
