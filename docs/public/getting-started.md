# Getting Started {#getting_started}

This page takes you from a fresh checkout to a running RADPx-OS image on two
targets — the x86_64 GRUB terminal and the ZuBoard-1CG (ZynqMP Cortex-A53)
under QEMU. It is the narrative companion to the command reference in the
repository README; read this first if you have not built RADPx-OS before.

## What you are building

RADPx-OS is a POSIX-inspired kernel (RADKernel) that runs both as a host
simulator and on real hardware. The same kernel core boots on x86_64 and on the
Cortex-A53; the target-specific pieces live under `RADKernel/platforms/` and
`RADKernel/boards/`. Two things you can produce in a few minutes:

- an **x86_64 terminal ISO** you can boot in a VM, and
- a **ZuBoard-1CG serial image** you can boot in QEMU to an interactive login.

## Prerequisites

- A Linux host (Debian/Ubuntu is the tested baseline).
- `cmake` and a host C++20 toolchain (for the host kernel tests).
- `qemu-system-aarch64` (for the ZuBoard QEMU smokes).
- The `radbuild` driver — install the frozen release once:

  ```bash
  sudo apt install ./radbuild_0.2.1_amd64.deb   # from a RadBuild release asset
  ```

  `radbuild` cross-builds the kernel and userland, assembles the disk images,
  and runs each target's smoke. You do not need a Python checkout to use it.

## 1. Prove the core builds (host tests, ~1 minute)

The fastest confidence check needs no emulator — it builds the portable kernel
core and runs the host unit tests:

```bash
cmake -S . -B build-host -DRADPX_OS_BUILD_TESTS=ON
cmake --build build-host -j2
./build-host/tests/RADKernelTests
```

A green run here means the kernel core, VFS, and filesystem code compile and
pass on your host before you involve any target hardware.

## 2. Boot the x86_64 terminal (ISO, ~2–3 minutes)

Build and smoke the default x86_64 GRUB terminal profile:

```bash
radbuild build os --settings settings.json --json-events
```

RadBuild resolves `settings.json` as an OS build configuration, produces the
ISO under `build/embedded/x86_64_grub_terminal/`, and runs the terminal VM
smoke. To run just the ISO smoke against an existing build:

```bash
RAD_X86_UI_PROFILE=terminal tools/embedded/x86_64_grub_slint_smoke.sh
```

The `settings.wm.json` profile builds the Slint-backed RADCompositor desktop
shell instead of the plain terminal image. That shell boots a multi-window
desktop — a gradient dock with an app launcher plus Terminal, File Explorer,
and Text Editor windows — rendered by the freestanding Slint software renderer
in the kernel. Run it with `-enable-kvm -cpu host` for smooth window drag:

```bash
RAD_X86_UI_PROFILE=wm tools/embedded/x86_64_grub_slint_smoke.sh
```

## 3. Boot the ZuBoard-1CG in QEMU (A53, ~2–3 minutes)

The ZuBoard profile cross-builds the A53 kernel and userland, assembles the SD
image, and runs the QEMU marker smoke that boots to an interactive serial
login:

```bash
radbuild build os --settings settings.ci.json --system rad-zuboard-1cg-qemu-ci --json-events
```

Once an image exists, you can run the individual A53 smokes directly:

```bash
tools/embedded/rad_zuboard_1cg/qemu_marker_smoke.sh   # ordered boot-marker gate
tools/embedded/rad_zuboard_1cg/qemu_login_smoke.sh    # interactive login / ls / cat / ps
tools/embedded/rad_zuboard_1cg/qemu_smp_smoke.sh      # second A53 core under -smp 2
```

The marker gate reads `tools/embedded/rad_zuboard_1cg/expected-markers.txt` and
requires each `RAD_*` milestone marker to appear in order on the serial log.

## Where to go next

- @ref api_structure — the stable `radkernel.h` C ABI and how the subsystem
  headers fit together.
- @ref architecture — the source tree: core, fs, drivers, platforms, boards,
  runtime, hal.
- @ref networking and @ref device_tree_guide, plus the ZuBoard-1CG bring-up
  notes, for target-specific detail.
- The public Crimson API docs are published through RadicalPackages under
  `docs/radpx-os/0.1.4/api/`.
