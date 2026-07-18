# RADPx ZUBoard 1CG Serial Target

This target builds a serial-only AArch64 RADPx payload for the Avnet/Tria
ZUBoard 1CG. It is intended to be loaded by U-Boot with `bootelf` after FSBL,
PMU firmware, and optional TF-A/BL31 have initialized the Zynq UltraScale+ MPSoC
processing system.

Current scope:

- A53 core 0 entry.
- Secondary A53 parking.
- Cadence PS UART console at 115200 8N1.
- Generic ARM timer based delays.
- 128-task / 128-user-process A53 configuration with 512 KiB kernel task
  stacks for parity with the current x86 direction.
- Embedded `/bin/init`, `/bin/radsh`, `/bin/sh` smoke payloads.

Out of scope for this first bring-up:

- SD/eMMC filesystem access inside RADPx.
- USB, Ethernet, framebuffer, or PL runtime drivers.
- R5 AMP scheduling or mailbox ownership.

Expected serial markers include:

- `RADIX_ZYNQMP_ENTRY_OK`
- `RADIX_ZUBOARD_UART_OK`
- `RADIX_ZUBOARD_A53_CORE0_OK`
- `RADIX_ZUBOARD_A53_PROCESS_PARITY_OK`
- `RADIX_ZUBOARD_SERIAL_READY`

Build locally:

```sh
make -C tools/embedded/radix_zuboard_1cg
```

RadBuild owns the higher-level ZuBoard flow that generates or reuses an XSA,
builds boot firmware and U-Boot, and stages an SD boot directory.
