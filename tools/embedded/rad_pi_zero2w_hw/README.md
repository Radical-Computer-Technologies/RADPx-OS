# RADPx-OS — Raspberry Pi Zero 2 W hardware boot kit

Everything needed to run RADPx-OS on a **real** Pi Zero 2 W and drive it from a
host (the same edit → build → boot → check-markers loop we use under QEMU). The
target is a Pico **DirtyJTAG** probe providing both JTAG and a UART bridge, so the
board is driven over one probe.

> **Untested on hardware in-repo.** Every register/address here is real-silicon
> correct and matches the driver refs, but the physical bring-up (TAPID, debug
> bases, DirtyJTAG timing, USB-boot OTP) must be confirmed on first connect.

## What's here

| File | Purpose |
|---|---|
| `config.txt` | Pi boot config: `arm_64bit`, PL011 to GPIO14/15 (`disable-bt`), `enable_jtag_gpio`. |
| `fetch_firmware.sh` | Pull the GPU boot firmware (+ `--wifi` for the CYW43438 blob). |
| `make_boot_sd.sh` | Build a full bootable SD image (firmware + kernel8.img + ext4 rootfs). |
| `make_usbboot_otp_sd.sh` | One-time OTP programmer SD to **enable USB boot** (read the warnings). |
| `rpiboot_iterate.sh` | Fast loop: build + serve a fresh `kernel8.img` over USB (no SD reflash). |
| `serial_console.py` | pyserial console + marker gate + login driver (mirrors the QEMU smoke). |
| `openocd_pico_dirtyjtag_bcm2837.cfg` | OpenOCD: DirtyJTAG → BCM2837 4× Cortex-A53. |
| `jtag_debug.gdb` | GDB helpers incl. the **layout-bug watchpoint recipe** + console-over-JTAG. |

## Wiring (Pi 40-pin header → Pico DirtyJTAG)

- **UART console** — PL011, routed to the header by `disable-bt`:
  `GPIO14` TXD (pin 8) → probe RX, `GPIO15` RXD (pin 10) → probe TX, GND.
- **JTAG** — `enable_jtag_gpio=1` puts it on GPIO22-27 (alt4):
  `22`=TRST, `23`=RTCK, `24`=TDO, `25`=TCK, `26`=TDI, `27`=TMS, + GND.
- **USB (data) port** → host: used for rpiboot (device-mode boot).
- **microSD** inserted: holds the ext4 rootfs (mounted by the kernel via SDHOST).

No pin conflicts between UART (14/15), JTAG (22-27), our WiFi/SDIO (34-39) and
SDHOST (48-53). The one caveat is GPIO **boot-mode** select (below).

## First boot (SD)

```
./fetch_firmware.sh              # + --wifi to also stage the CYW43438 blob
./make_boot_sd.sh                # -> artifacts/rad/pi-zero2w/pi-zero2w-boot.img
sudo dd if=artifacts/rad/pi-zero2w/pi-zero2w-boot.img of=/dev/sdX bs=4M conv=fsync
./serial_console.py --gate       # power on; expect the 118-marker gate
./serial_console.py --login      # confirm RAD_LOGIN_OK
```

## Fast iteration (USB boot, no SD reflash)

Enable USB boot once, then serve fresh kernels over USB (rootfs stays on the SD):

```
./make_usbboot_otp_sd.sh         # boot this SD once, power-cycle -> OTP set (PERMANENT)
# ...swap the rootfs SD back in...
./rpiboot_iterate.sh             # builds kernel8.img, serves it via rpiboot
./serial_console.py --gate
```

`program_usb_boot_mode=1` (what `make_usbboot_otp_sd.sh` sets) enables USB boot
without consuming GPIOs. To also **force** USB and skip the 5 s SD timeout there's
GPIO boot-mode (`--gpio-bootmode 1|2`), but both banks collide on the Zero 2 W
(low bank 22-26 == JTAG; high bank 39-43 == internal WiFi/SD) — the script spells
out the trade-off. It's **one-time and irreversible**, so prefer the plain mode.

## Console over JTAG (only the probe, no UART wire) — opt-in

Build the kernel with the debug ring and read/inject the console over the JTAG
memory port:

```
make -C tools/embedded/rad_pi_zero2w RAD_PI_EXTRA_DEFINES=-DRAD_PI_JTAG_CONSOLE
openocd -f tools/embedded/rad_pi_zero2w_hw/openocd_pico_dirtyjtag_bcm2837.cfg
aarch64-none-elf-gdb tools/embedded/rad_pi_zero2w/RADKRN.ELF -x .../jtag_debug.gdb
(gdb) jtagcon                    # drain console output
(gdb) jtagsend "root\r"          # inject input
```

It's **off by default** on purpose: adding kernel code shifts the memory layout,
and there's an open layout-sensitivity bug (see below) — keeping it opt-in leaves
the shipped image untouched.

## The open bug JTAG is for

Linking `bcm283x_wifi.o` deterministically breaks the interactive login via a
latent, layout-sensitive corruption (one instance — the page-allocator/image
overlap — is already fixed with `RAD_A53_USABLE_FLOOR`; a second, in the shifted
newlib `.bss`, remains). `jtag_debug.gdb`'s `watchsym` + the documented procedure
are exactly how to catch the overrunning write on hardware. Re-link the WiFi
driver (Makefile `OBJS` + the probe call in `radkernel_hal_bcm283x.cpp`) to
reproduce the failing build, then watchpoint.

## Prereqs on the host

`rpiboot` (installed), `python3` + `pyserial`, `sfdisk`/`mkfs.vfat`/`mkfs.ext4`/
`losetup` (image build), `openocd` **built with the `dirtyjtag` driver**, and the
`aarch64-none-elf` GDB from the RADLib toolchain.
