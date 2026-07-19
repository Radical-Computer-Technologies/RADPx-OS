#!/usr/bin/env bash
#
# Build the Raspberry Pi Zero 2 W SD-card image (FAT boot p1 + ext4 rootfs p2).
#
# The bcm283x A53 kernel shares the ZuBoard/ZynqMP rootfs userland byte-for-byte
# (same aarch64 radinit / login / rash / configs), so this derives the Pi SD
# image from the ZuBoard rootfs image and pads it to the power-of-2 size QEMU's
# sd-card model requires. If the ZuBoard rootfs image is not present, build it
# first (see tools/embedded/rad_zuboard_1cg + RadBuild), then re-run this.
#
# Output: artifacts/rad/pi-zero2w/pi-zero2w-sd.img  (512 MiB, MBR: p1 FAT, p2 ext4)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC_IMG="${RAD_PI_ROOTFS_SRC:-$ROOT/artifacts/rad/zuboard-1cg-serial/rad-zuboard-1cg.img}"
OUT_DIR="$ROOT/artifacts/rad/pi-zero2w"
OUT_IMG="$OUT_DIR/pi-zero2w-sd.img"
# QEMU sd-card requires a power-of-2 byte size; 512 MiB comfortably holds the
# 64 MiB FAT + 256 MiB ext4 layout with headroom.
SD_BYTES=$((512 * 1024 * 1024))

if [[ ! -f "$SRC_IMG" ]]; then
    echo "rad_pi_zero2w_mkimage: source rootfs image not found: $SRC_IMG" >&2
    echo "  Build the ZuBoard rootfs first, or set RAD_PI_ROOTFS_SRC to a" >&2
    echo "  partitioned image (MBR: p1 FAT, p2 ext4 rootfs)." >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
cp -f "$SRC_IMG" "$OUT_IMG"
truncate -s "$SD_BYTES" "$OUT_IMG"

# Sanity: confirm the MBR still describes p1 (FAT) + p2 (Linux/ext4).
if command -v sfdisk >/dev/null 2>&1; then
    if ! sfdisk -d "$OUT_IMG" 2>/dev/null | grep -q 'type=83'; then
        echo "rad_pi_zero2w_mkimage: WARNING - no ext4 (type=83) partition found in image" >&2
    fi
fi

echo "rad_pi_zero2w_mkimage: wrote $OUT_IMG ($(stat -c %s "$OUT_IMG") bytes)"
