# RADix Pi Zero 2 W Standalone Payload

This target builds the RADix-owned Pi Zero 2 W kernel payload. Raspberry Pi
firmware can boot it directly as `kernel8.img`; the optional RADix/Circle
second-stage loader can also place it on the FAT boot partition as
`RADIXKRN.IMG` and jump through `rad_boot_handoff_t`.

The payload does not link Circle. It expects a `rad_boot_handoff_t` pointer in
`x0`; if it is booted directly as `kernel8.img`, it creates a fallback handoff
for early serial/framebuffer bring-up.

```bash
make -C tools/embedded/radix_pi_zero2w
```

Current checkpoint:

- RAD-owned BCM283x UART console
- RAD-owned system timer reads and busy sleep
- RAD-owned mailbox framebuffer registration
- RAD-owned `/dev/mmcblk0` registration scaffold with QEMU smoke sector reads
- RAD-owned `/dev/usb0` host-info scaffold
- RAD-owned `/dev/input/event0` synthetic HID input scaffold
- AArch64 process-architecture parity markers for EL0, SVC, user-copy, execve,
  fork, and COW page-fault gates
- Slint/RADCompositor boot-shell, window-manager, terminal-window, and dirty
  present markers
- RADix runtime boot markers and terminal commands

The real BCM283x eMMC command engine, DWC OTG USB host enumeration, hardware
HID input, AArch64 page-table/trap/fork/COW implementation, and Slint shell
rendering on the Pi framebuffer are still the next pieces before this target
reaches x86 parity.
