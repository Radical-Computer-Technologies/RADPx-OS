#!/usr/bin/env bash
#
# Build a fully bootable Pi Zero 2 W SD image: FAT boot partition (GPU firmware +
# config.txt + kernel8.img) plus the ext4 rootfs partition (from the shared
# rad_pi_zero2w_mkimage.sh). Writes artifacts/rad/pi-zero2w/pi-zero2w-boot.img,
# which you dd to a microSD. Use this for the first bring-up and whenever the
# rootfs changes; for fast kernel-only iteration prefer rpiboot_iterate.sh.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PAYLOAD_DIR="$ROOT/tools/embedded/rad_pi_zero2w"
FW="$HERE/firmware"
OUT="$ROOT/artifacts/rad/pi-zero2w/pi-zero2w-boot.img"

[[ -f "$FW/start.elf" ]] || { echo "run ./fetch_firmware.sh first" >&2; exit 1; }

echo "== build kernel8.img + rootfs image =="
make -C "$PAYLOAD_DIR" >/dev/null
bash "$ROOT/tools/embedded/rad_pi_zero2w_mkimage.sh" >/dev/null
cp "$ROOT/artifacts/rad/pi-zero2w/pi-zero2w-sd.img" "$OUT"

echo "== populate the FAT boot partition (p1) =="
# p1 starts at sector 2048 (1 MiB) per the shared image layout.
LOOP="$(sudo losetup --show -f -o $((2048*512)) "$OUT")"
MNT="$(mktemp -d)"
cleanup() { sudo umount "$MNT" 2>/dev/null || true; sudo losetup -d "$LOOP" 2>/dev/null || true; rmdir "$MNT" 2>/dev/null || true; }
trap cleanup EXIT
sudo mount "$LOOP" "$MNT"
sudo cp "$FW/bootcode.bin" "$FW/start.elf" "$FW/fixup.dat" "$MNT/"
sudo cp "$HERE/config.txt" "$MNT/config.txt"
sudo cp "$PAYLOAD_DIR/RADKRN.IMG" "$MNT/kernel8.img"
# Optional: stage the WiFi firmware where the driver's loader expects it.
[[ -f "$FW/brcmfmac43430-sdio.bin" ]] && sudo cp "$FW"/brcmfmac43430-sdio.* "$MNT/" || true
sync
echo "  boot partition contents:"; ls -la "$MNT"
cleanup; trap - EXIT

echo "done: $OUT"
echo "flash with:  sudo dd if=$OUT of=/dev/sdX bs=4M conv=fsync status=progress"
