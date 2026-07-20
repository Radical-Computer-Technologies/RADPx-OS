#!/usr/bin/env bash
#
# Build a small "OTP programmer" SD image that, when booted ONCE and then
# power-cycled, permanently enables USB boot on the Pi Zero 2 W. After that the
# board can boot our kernel over USB via rpiboot_iterate.sh with no SD boot files.
#
#   !!! THE OTP IS ONE-TIME-PROGRAMMABLE AND CANNOT BE UNDONE. Read this first. !!!
#
# Two levers (config.txt directives processed by start.elf at boot):
#
#  1. program_usb_boot_mode=1   [default, SAFE]
#     Enables USB boot in the OTP. With no bootable files on the SD afterwards,
#     the ROM falls through to USB device boot (rpiboot) after a ~5 s SD timeout.
#     No GPIO pins are consumed -- does NOT conflict with JTAG or WiFi.
#     Verify after: `vcgencmd otp_dump | grep 17:` -> 0x3020000a means programmed.
#
#  2. program_gpio_bootmode=N   [OPT-IN, --gpio-bootmode 1|2, PERMANENT + PIN COST]
#     Permanently allocates a 5-GPIO bank so a pin state selects/disables boot
#     modes at power-on (lets you FORCE USB and skip the 5 s SD timeout). BUT on
#     the Zero 2 W both banks collide with this rig:
#       N=1 low bank  GPIO22-26  == our JTAG pins (GPIO22-27, enable_jtag_gpio)
#       N=2 high bank GPIO39-43  == internal WiFi/SD pins (WiFi is GPIO34-39)
#     The bank is only READ at power-on (before start.elf sets JTAG alt4), so if
#     the DirtyJTAG lines are high-Z at power-on (OpenOCD not yet attached) a
#     pull-up can still select USB -- but it is fragile. Prefer lever 1 unless you
#     specifically need to eliminate the 5 s timeout. If you use it, N=1 (low bank)
#     is the lesser evil since JTAG is idle at power-on; wire a pull-up on the bank
#     bit that disables SD boot.
#
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
FW="$HERE/firmware"
OUT="$ROOT/artifacts/rad/pi-zero2w/pi-zero2w-usbboot-otp.img"
GPIO_BOOTMODE=""
[[ "${1:-}" == "--gpio-bootmode" ]] && GPIO_BOOTMODE="${2:?bank 1 or 2}"

[[ -f "$FW/start.elf" ]] || { echo "run ./fetch_firmware.sh first" >&2; exit 1; }

echo "== assemble 32 MiB FAT OTP-programmer image =="
rm -f "$OUT"; truncate -s $((32*1024*1024)) "$OUT"
printf 'label: dos\nstart=2048, type=c\n' | sfdisk "$OUT" >/dev/null
LOOP="$(sudo losetup --show -f -o $((2048*512)) "$OUT")"
MNT="$(mktemp -d)"
cleanup() { sudo umount "$MNT" 2>/dev/null || true; sudo losetup -d "$LOOP" 2>/dev/null || true; rmdir "$MNT" 2>/dev/null || true; }
trap cleanup EXIT
sudo mkfs.vfat "$LOOP" >/dev/null
sudo mount "$LOOP" "$MNT"
sudo cp "$FW/bootcode.bin" "$FW/start.elf" "$FW/fixup.dat" "$MNT/"
{
    echo "# One-time OTP programmer -- boot once, then power-cycle, then remove this SD."
    echo "program_usb_boot_mode=1"
    [[ -n "$GPIO_BOOTMODE" ]] && echo "program_gpio_bootmode=$GPIO_BOOTMODE"
    echo "enable_uart=1"
    echo "dtoverlay=disable-bt"
} | sudo tee "$MNT/config.txt" >/dev/null
sudo cat "$MNT/config.txt"
sync; cleanup; trap - EXIT

echo "done: $OUT"
echo "  1) dd to a spare microSD, boot the Pi once, WAIT ~10 s, then power-cycle."
echo "  2) (optional) reboot with a normal OS and run: vcgencmd otp_dump | grep 17:"
echo "     0x3020000a == USB boot enabled."
echo "  3) Remove this SD. rpiboot_iterate.sh will now boot the board over USB."
