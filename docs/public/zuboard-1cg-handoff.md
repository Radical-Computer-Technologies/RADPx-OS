# ZuBoard 1CG A53 Handoff Status

This note captures the current handoff point for the RADPx ZuBoard/ZynqMP A53
bring-up.

## What Works

- `tools/embedded/radix_zuboard_1cg/boot.S` masks interrupts, selects core 0,
  parks nonzero MPIDR cores in `wfe`, clears BSS, installs a clean stack, and
  enters the C++ kernel.
- QEMU smoke works against `qemu-system-aarch64 -M xlnx-zcu102` with
  single-threaded TCG. The local helper is:

```bash
RADIX_QEMU_BUILD=0 RADIX_QEMU_TIMEOUT=40 \
  tools/embedded/radix_zuboard_1cg/run-qemu.sh
```

- The QEMU profile uses `RADIX_ZYNQMP_SDHCI_BASE=0xff160000u`; hardware keeps
  the ZuBoard default `0xff170000u`.
- The ZynqMP HAL initializes the Cadence PS UART, registers `/dev/console`,
  creates serial-backed `/dev/tty0`, initializes SDHCI, registers
  `/dev/mmcblk0`, reads the MBR, and registers `/dev/mmcblk0p1` and
  `/dev/mmcblk0p2`.
- The A53 MMU now identity-maps the full 32-bit physical address window and
  treats the high ZynqMP MMIO range as device memory.
- The kernel mounts ext4 from `/dev/mmcblk0p2`, finds `/bin/init`, creates the
  user process, binds stdio to `/dev/tty0`, and reaches the serial login-ready
  handoff point.
- RadBuild has a `radix-zuboard-1cg-qemu` system profile and emits
  `artifacts/radix/zuboard-1cg-qemu/radix-zuboard-1cg-qemu.img` as a padded
  512 MiB QEMU SD image.

## Verified Marker Path

The current QEMU smoke reaches:

```text
RADIX_ZUBOARD_SD_OK
RADIX_ZUBOARD_MMCBLK0_OK
RADIX_ZUBOARD_PARTITION_BOOT_OK
RADIX_ZUBOARD_PARTITION_ROOT_OK
RADIX_AARCH64_MMU_ON_OK
RADIX_AARCH64_EXCEPTION_VECTORS_OK
RADIX_EXT4_MOUNT_OK
RADIX_ZUBOARD_EXT4_ROOT_OK
RADIX_ROOTFS_INIT_FOUND
RADIX_AARCH64_USER_PROCESS_SPAWN_OK
RADIX_LOGIN_SPAWN_OK
RADIX_ZUBOARD_SERIAL_LOGIN_READY
RADIX_ZUBOARD_SERIAL_READY
RADIX_AARCH64_USERMODE_ENTER_OK
```

## Current Blocker

The first ext4-rootfs user process enters EL0 and immediately traps back to EL1
at the first instruction of `/bin/init`:

```text
[ERR] RADIX_AARCH64_USER_EXCEPTION el=1 esr=0x2000000 far=0x0 elr=0x80000000
RADIX_AARCH64_USER_EXCEPTION_EXIT_OK
```

`0x80000000` is the `_start` address of the generated AArch64 user ELF. The
first instruction is valid (`ldr x0, [sp]`), so the next pass should focus on
the EL0 exception-return and user translation state rather than SD, ext4, or
process spawning.

## Recommended Next Steps

1. Split the AArch64 vector entries so the trap log records the exact vector
   slot: current/lower EL, AArch64/AArch32, SP0/SPx, sync/IRQ/FIQ/SError.
2. Dump `SCTLR_EL1`, `TCR_EL1`, `MAIR_EL1`, `TTBR0_EL1`, `TTBR1_EL1`,
   `CPACR_EL1`, and the user leaf PTE for `0x80000000` before `eret`.
3. Add a minimal one-instruction user smoke ELF that only performs `svc exit`.
   If that also traps at `0x80000000`, the fault is the EL0 return or execute
   permission state. If it runs, the issue is in the generated init stack or
   `libradixc` entry path.
4. Change ELF segment mapping to honor permissions: executable text should be
   user-readable/executable and not writable; data should be writable and UXN.
5. After EL0 executes reliably, verify `/dev/tty0` login input over QEMU serial
   and then repeat on hardware through U-Boot `bootelf`.

