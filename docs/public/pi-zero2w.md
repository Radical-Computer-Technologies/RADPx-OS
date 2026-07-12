# Pi Zero 2 W Bring-Up {#pi_zero2w}

RADix-OS now has the first pieces of the Raspberry Pi Zero 2 W port. Raspberry
Pi firmware remains the real first-stage boot path: the GPU ROM, `bootcode.bin`,
and `start4.elf` initialize RAM, read `config.txt`, load the ARM payload, and
release the AArch64 cores. The RADix-specific second stage is optional, but it
is still useful as a maintenance loader for future boot menus, kernel selection,
reflash tools, handoff validation, and hardware policy.

The intended final shape is:

- Pi firmware performs first-stage hardware boot.
- A narrow Circle-based second-stage loader can load `RADIXKRN.IMG` from the
  FAT boot partition and jump through `rad_boot_handoff_t`.
- The standalone RADix payload uses the `bcm283x_pi` backend and does not link
  Circle.

## Required Loader State

The handoff is intentionally strict for physical silicon. Before jumping from
any second-stage loader to the RADix payload, the loader must:

- Park secondary cores 1-3 in a clean `wfe` loop owned by the loader handoff.
- Mask interrupts on core 0.
- Disable the MMU and data cache.
- Clean/invalidate data cache state covering the payload and handoff record.
- Invalidate the instruction cache and TLB.
- Jump to the RADix entry in AArch64 EL1.

`radboot_validate_handoff()` rejects Pi handoff records that do not declare
these conditions through the `RAD_BOOT_HANDOFF_FLAG_*` safety bits.

## Current Payload

Build the standalone payload with:

```bash
make -C tools/embedded/radix_pi_zero2w
```

The output files are:

- `tools/embedded/radix_pi_zero2w/RADIXKRN.ELF`
- `tools/embedded/radix_pi_zero2w/RADIXKRN.IMG`

Run the RADix-owned QEMU smoke test with:

```bash
tools/embedded/radix_pi_zero2w_smoke.sh
```

The script uses `qemu-system-aarch64 -M raspi3b` because QEMU does not expose a
Pi Zero 2 W machine model. If `qemu-system-aarch64` is not installed, the smoke
test downloads and extracts Ubuntu's `qemu-system-arm` package under
`.radbuild/qemu-arm` and reuses it on later runs. A passing run verifies the
early BCM283x HAL, PL011 UART, mailbox framebuffer, handoff validation, payload
entry, block and FAT mount scaffolds, USB/input scaffolds, framebuffer dirty
present, portable A53 boot/process/COW checks, and Slint/RADCompositor parity
markers.

The current RAD-owned backend includes:

- A platform split under `RADixKernel/platforms`: portable A53 code in
  `platforms/a53` and Pi-specific BCM283x drivers in `platforms/a53/bcm283x`.
- PL011 serial console registration.
- System timer reads and busy sleep.
- Interrupt enable/disable hooks for the BCM interrupt controller.
- Mailbox property framebuffer allocation and `/dev/fb0` registration.
- `/dev/mmcblk0` block-device registration scaffold with in-memory sector
  backing for QEMU smoke reads.
- `/dev/usb0` host-info scaffold and `/dev/input/event0` synthetic HID input
  event path.
- A portable A53 process model that registers process arch hooks, performs
  kernel process-table fork/fd-clone/wait checks, and exercises deterministic
  parent/child COW isolation before the future privileged MMU implementation
  takes over the same interface.
- Slint/RADCompositor boot-shell, window-manager, terminal-window, and
  compositor-damage smoke markers.

## Circle Loader Gate

Run the combined loader/payload smoke test with:

```bash
tools/embedded/qemu_pizero2w_smoke.sh
```

The Circle loader source now builds a narrow second-stage handoff record and
emits loader-intent markers when serial output is visible. On this workstation
the Circle image currently reaches QEMU without serial markers, so the combined
smoke script treats the Circle image as a build gate and then runs the
standalone RADix payload gate. Physical Pi hardware remains the next authority
for validating the actual Circle FAT load and jump sequence.

## Current Limits

This page is experimental. The portable A53 layer now owns the boot/process/COW
contract, but privileged AArch64 exception-vector entry, real EL0 execution,
hardware page tables, and user ELF replacement are not complete yet. The eMMC,
USB, HID, and Slint shell paths are still QEMU-visible scaffolds rather than
complete physical drivers. The next Pi passes need the real BCM283x eMMC command
engine, real DWC OTG host enumeration, hardware keyboard/mouse input,
second-stage FAT load/jump validation on silicon, and full AArch64
page-table/trap/fork/exec/COW behavior.

Physical Pi Zero 2 W hardware remains the authority for mailbox framebuffer and
SD behavior.
