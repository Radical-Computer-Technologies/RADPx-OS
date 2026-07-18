# Architecture & Layout {#architecture}

RADPx-OS keeps one portable kernel core (RADKernel) shared across every target
and pushes hardware-specific code to the edges. This page maps the source tree
so you know where a given concern lives before you change it.

## Design shape

- **One core, many backends.** The portable kernel logic is arch-neutral. Each
  target supplies a thin platform/board layer and a HAL; the core does not
  change per target.
- **Stable C ABI.** Everything the kernel exposes goes through `radkernel.h`
  (see @ref api_structure). Subsystem headers under
  `RADKernel/headers/radkernel/` (`rad_vfs.h`, `rad_net.h`, `rad_spi.h`, …) each
  include that ABI so a driver or app pulls in only what it uses.
- **Shared services, not copies.** Filesystems, drivers, and the freestanding
  runtime are written once under shared trees and linked by every target that
  needs them, rather than being duplicated per platform.

## The `RADKernel/` tree

| Directory | Role |
|-----------|------|
| `core/` | The portable kernel core. `RADKernel.cpp` is the host-simulator backend; `RADKernel_runtime.cpp` is the embedded/freestanding backend. Selected by `RAD_KERNEL_BACKEND`. |
| `headers/radkernel/` | The public C ABI: `radkernel.h` plus the per-subsystem headers (`rad_vfs.h`, `rad_task.h`, `rad_net.h`, `rad_tty.h`, `rad_spi.h`, `rad_irq.h`, …). |
| `boot/` | `radboot` — the `rad_boot_info_t` hand-off that platform code fills before `rad_kernel_init()`. |
| `fs/` | Portable, block-device filesystems shared by all targets: `ext4`, `ext2`, `fat`. |
| `drivers/` | Portable device drivers. `drivers/net/gem_cadence.cpp` is the Cadence GEM NIC used on ZynqMP. |
| `platforms/` | Arch- and SoC-specific code. `x86_64/` (CPU, VM, user, storage, boot). `a53/` for Cortex-A53, with the `zynqmp/` (ZuBoard/ZynqMP) and `bcm283x/` (Pi) SoC HALs beneath it. |
| `boards/` | Per-board kernel entry sources. `boards/zuboard_1cg/` holds that board's `kernel.cpp`, `boot.S`, and `linker.ld`. |
| `runtime/` | The freestanding kernel runtime (`freestanding_runtime.cpp`) — heap, `malloc`/`free`, formatting, mem/string — for targets built `-nostdlib`. |
| `hal/` | Toolchain/SDK glue kept out of the core: `hal/circle/` (Circle SDK + newlib runtime glue) and `hal/rp235x_pico/`. |
| `subsystems/` | Cross-cutting runtime subsystems. `RADOverlayRuntime.cpp` binds the device-tree/overlay description to buses, devices, framebuffers, and interrupt domains. |
| `modules/` | Reserved for loadable-module content. |

## Everything else

- `tools/embedded/` — the buildable target glue and smoke scripts (Makefiles,
  CMake, QEMU runners). The x86_64 GRUB terminal/WM profiles and the
  `rad_zuboard_1cg/` ZuBoard target live here; the board *source* is under
  `RADKernel/boards/`, while the *build glue* stays here.
- `userland/` — RADPx-OS userspace program source (init, the `rash` shell,
  login, coreutils shims).
- `libs/` — userspace libraries: `libradc` (the RADPx-OS C library) and the
  ncurses port.
- `services/` — the daemon/service set (`RADServiceManager`/radinit and friends).
- `tests/` — host-side kernel unit tests (`RADKernelTests`).
- `docs/` — this documentation set (`docs/public/`) and the Doxygen config.

## How a target is assembled

A target build links the **core** (`RADKernel_runtime.cpp`) + the **shared
services** it needs (`fs/`, `drivers/`, `runtime/` or a HAL) + its **platform**
(`platforms/<arch>/`) + its **board entry** (`boards/<board>/`). For example,
the ZuBoard-1CG image links the A53 platform, the ZynqMP SoC HAL, the ext4
filesystem, the Cadence GEM driver, and the `zuboard_1cg` board entry. Because
the core and shared services are identical across targets, parity work on one
target (a filesystem fix, a syscall) benefits the others without a port.

See @ref getting_started to build and boot a target, and the
[ZuBoard-1CG](zuboard-1cg.md) notes for the A53 bring-up specifics.
