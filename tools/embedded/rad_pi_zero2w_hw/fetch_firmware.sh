#!/usr/bin/env bash
#
# Fetch the Raspberry Pi GPU boot firmware needed on the Zero 2 W boot partition
# (and, optionally, the CYW43438 WiFi firmware blob for the driver). These are
# redistributable but not vendored in-repo; this pulls them into ./firmware/.
#
#   bootcode.bin  start.elf  fixup.dat   -> GPU second-stage that loads kernel8.img
#   brcmfmac43430-sdio.{bin,txt,clm_blob} (optional, --wifi) -> CYW43438 firmware
#
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HERE/firmware"
FW_REF="${RAD_PI_FW_REF:-master}"
FW_BASE="https://raw.githubusercontent.com/raspberrypi/firmware/$FW_REF/boot"
# WiFi firmware lives in the nonfree tree (Broadcom/Cypress, redistributable).
WIFI_BASE="https://github.com/RPi-Distro/firmware-nonfree/raw/master/debian/config/brcm80211"

mkdir -p "$OUT"
get() { echo "  fetch $2"; curl -fsSL "$1" -o "$OUT/$2"; }

echo "== GPU boot firmware ($FW_REF) -> $OUT"
get "$FW_BASE/bootcode.bin" bootcode.bin
get "$FW_BASE/start.elf"    start.elf
get "$FW_BASE/fixup.dat"    fixup.dat

if [[ "${1:-}" == "--wifi" ]]; then
    echo "== CYW43438 WiFi firmware (brcmfmac43430) -> $OUT"
    # The Zero 2 W's chip (43430, rev 1) uses the Cypress cyfmac43430 image.
    get "$WIFI_BASE/cypress/cyfmac43430-sdio.bin"      brcmfmac43430-sdio.bin
    get "$WIFI_BASE/brcm/brcmfmac43430-sdio.txt"       brcmfmac43430-sdio.txt
    get "$WIFI_BASE/cypress/cyfmac43430-sdio.clm_blob" brcmfmac43430-sdio.clm_blob
fi

echo "done. firmware in $OUT"
ls -la "$OUT"
