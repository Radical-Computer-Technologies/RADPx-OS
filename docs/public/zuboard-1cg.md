# ZuBoard 1CG Serial Bring-Up

RADix-OS includes an experimental serial-only target for the Avnet/Tria ZuBoard
1CG. This is the first Zynq UltraScale+ MPSoC platform target and is intended
to prove A53 boot handoff before SD, USB, Ethernet, framebuffer, PL runtime, or
R5 AMP scheduling work.

Current bring-up state and next debugging steps are tracked in
[`zuboard-1cg-handoff.md`](zuboard-1cg-handoff.md).

## Boot Model

The first pass uses the standard ZynqMP boot chain:

1. FSBL initializes the processing system and DDR from an XSA.
2. PMU firmware and optional TF-A/BL31 establish the firmware handoff state.
3. U-Boot loads `radix-zuboard.elf` from the ext4 root partition.
4. U-Boot enters RADix with `bootelf`.

RadBuild owns the repeatable flow around this chain. It can stage the local
`../ZUBoard-1CG_RT` reference XSA for quick bring-up, generate a PS-only XSA
when Avnet board files are available, write XSCT firmware scripts, write a
Bootgen BIF, and stage SD-card boot files.

## Current Kernel Scope

The `zynqmp_zuboard_1cg` backend currently provides:

- A53 core-0 boot entry with all nonzero MPIDR cores parked in `wfe`.
- Secondary A53 parking markers for the dual-core ZuBoard target.
- Cadence PS UART console at 115200 8N1.
- ZynqMP SDHCI block registration as `/dev/mmcblk0` with MBR partitions.
- ext4 rootfs mount from `/dev/mmcblk0p2`.
- ARM generic timer delays.
- AArch64 exception vector install, EL1 MMU enable, 4 GiB identity kernel
  mapping, and high ZynqMP MMIO mapped as device memory.
- A53 process scale aligned with the x86 early target: 128 kernel tasks, 128
  architecture-tracked user processes, and 512 KiB kernel task stacks.
- `/bin/init`, `/bin/login`, `/bin/rash`, `/bin/sh`, and `libradixc.rso` in the
  ext4 rootfs.

Expected serial markers include `RADIX_ZUBOARD_UART_OK` and
`RADIX_ZUBOARD_SERIAL_LOGIN_READY`. The QEMU smoke should also emit
`RADIX_AARCH64_MMU_ON_OK`, `RADIX_EXT4_MOUNT_OK`,
`RADIX_ZUBOARD_EXT4_ROOT_OK`, `RADIX_AARCH64_USERLAND_OK`, and
`RADIX_LOGIN_SPAWN_OK`.

## Build

```bash
make -C tools/embedded/radix_zuboard_1cg
```

Or create a RadBuild project:

```bash
radbuild project create \
  --non-interactive \
  --template radix-os-zuboard-serial \
  --project-name radix_zuboard \
  --workspace /path/to/workspace
radbuild build os --settings /path/to/workspace/radix_zuboard/settings.json
```

The generated artifact directory contains the RADix ELF/image, reference or
generated XSA files, boot scripts, and staging files for the SD FAT partition.

## QEMU Smoke

The local QEMU smoke uses QEMU's `xlnx-zcu102` machine model as a close ZynqMP
A53/SDHCI/UART proxy. QEMU exposes SDHCI at a different base address than the
hardware ZuBoard path, so the QEMU profile rebuilds the kernel with
`RADIX_ZYNQMP_SDHCI_BASE=0xff160000u` and pads the SD image to 512 MiB because
QEMU's SD backend requires a power-of-two card size.

```bash
radbuild build os --system radix-zuboard-1cg-qemu
tools/embedded/radix_zuboard_1cg/run-qemu.sh
```

For automated marker checks:

```bash
RADIX_QEMU_BUILD=0 RADIX_QEMU_TIMEOUT=45 \
  tools/embedded/radix_zuboard_1cg/run-qemu.sh
```

Use `RADIX_QEMU_SMP=1` for the first smoke. The boot assembly parks all cores
except core 0; once GIC and per-core scheduling are brought up for ZynqMP, the
QEMU profile can be expanded to validate secondary A53 startup.
