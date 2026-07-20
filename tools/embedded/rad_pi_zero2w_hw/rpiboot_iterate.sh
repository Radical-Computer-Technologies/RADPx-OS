#!/usr/bin/env bash
#
# Fast real-hardware iteration for the Pi Zero 2 W over USB, no SD reflash:
# builds the kernel, assembles a boot directory, and serves it with rpiboot so
# the board's ROM boots our fresh kernel8.img straight over the USB port. The
# ext4 rootfs stays on the inserted microSD (mounted by the kernel via SDHOST).
#
# Wiring for this mode:
#   - Pi micro-USB (the data port) -> this host  : USB device-mode boot (rpiboot)
#   - microSD inserted                            : holds the ext4 rootfs (p2)
#   - Console/JTAG via the Pico DirtyJTAG (see serial_console.py / OpenOCD cfg)
#
# Put the Pi into USB boot: newer Zero 2 W units try USB device boot with no SD
# boot files present; older units need program_usb_boot_mode=1 fused once (from an
# ordinary SD boot). rpiboot then waits for the board and serves BOOT_DIR.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PAYLOAD_DIR="$ROOT/tools/embedded/rad_pi_zero2w"
BOOT_DIR="${RAD_PI_BOOT_DIR:-$HERE/bootdir}"
FW="$HERE/firmware"

if [[ ! -f "$FW/start.elf" ]]; then
    echo "GPU firmware missing -- run ./fetch_firmware.sh first." >&2
    exit 1
fi

echo "== build kernel8.img =="
make -C "$PAYLOAD_DIR" >/dev/null
echo "  $(stat -c '%s bytes' "$PAYLOAD_DIR/RADKRN.IMG")"

echo "== assemble boot dir: $BOOT_DIR =="
rm -rf "$BOOT_DIR"; mkdir -p "$BOOT_DIR"
cp "$FW/bootcode.bin" "$FW/start.elf" "$FW/fixup.dat" "$BOOT_DIR/"
cp "$HERE/config.txt" "$BOOT_DIR/config.txt"
cp "$PAYLOAD_DIR/RADKRN.IMG" "$BOOT_DIR/kernel8.img"
ls -la "$BOOT_DIR"

echo "== rpiboot -d $BOOT_DIR  (connect/power the Pi now; Ctrl-C to stop) =="
echo "   Then watch the console with:  ./serial_console.py --gate"
exec rpiboot -d "$BOOT_DIR"
