# RADix Platform Layout

`platforms/a53` contains portable ARMv8-A/Cortex-A53 execution support: boot
normalization contracts, 4 KiB page-table setup, TTBR0 process roots, TTBR1
kernel-root setup, process architecture registration, user-copy/COW interfaces,
and generic A53 capability reporting. Code in this directory must not depend on
Broadcom peripheral addresses.

`platforms/a53/bcm283x` contains Raspberry Pi Zero 2 W / BCM283x platform
drivers: PL011, mailbox framebuffer, Broadcom interrupt routing, eMMC, DWC OTG,
and handoff binding.

`platforms/a53/zynqmp` contains Avnet/Tria ZUBoard 1CG / Zynq UltraScale+
MPSoC platform drivers. The first target is serial-only: Cadence PS UART,
generic-timer timekeeping, A53 core-0 boot, and secondary-A53 parking. SD,
USB, Ethernet, PL accelerators, and R5 AMP scheduling are intentionally later
platform services.

`platforms/x86` is reserved for the x86 parity implementation. The current x86
VM target still builds from `tools/embedded/x86_64_grub_slint`; files should move
here once the target wrapper is thin enough to avoid changing behavior during
the Pi parity work.
