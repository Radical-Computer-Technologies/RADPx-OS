#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PAYLOAD_DIR="$ROOT/tools/embedded/rad_pi_zero2w"
LOG="$ROOT/.radbuild/rad-pi-zero2w-payload-smoke.log"

# shellcheck source=/dev/null
source "$ROOT/tools/embedded/qemu_aarch64_env.sh"
QEMU="$(rad_resolve_qemu_system_aarch64 "$ROOT")"

make -C "$PAYLOAD_DIR" clean >/dev/null 2>&1 || true
make -C "$PAYLOAD_DIR" >/dev/null

# Build the ext4-rootfs SD image so the Pi boots the full userland (radinit ->
# services -> rootfs) and gates RAD_SERVICE_ROOTFS_OK etc. The image is derived
# from the shared ZuBoard rootfs (same aarch64 userland); if that source rootfs
# has not been built, the marker gate cannot be satisfied -- build it first.
SD_IMG="$ROOT/artifacts/rad/pi-zero2w/pi-zero2w-sd.img"
if ! bash "$ROOT/tools/embedded/rad_pi_zero2w_mkimage.sh"; then
    echo "rad_pi_zero2w_smoke: could not build the SD rootfs image (see above)." >&2
    echo "  The Pi marker gate requires the ext4 rootfs; build the ZuBoard rootfs first." >&2
    exit 1
fi

mkdir -p "$(dirname "$LOG")"
set +e
timeout "${RAD_PI_QEMU_TIMEOUT:-20s}" "$QEMU" \
    -M raspi3ap \
    -cpu cortex-a53 \
    -m 512M \
    -kernel "$PAYLOAD_DIR/RADKRN.IMG" \
    -drive "file=$SD_IMG,if=sd,format=raw" \
    -serial stdio \
    -display none \
    -no-reboot \
    >"$LOG" 2>&1
status=$?
set -e

if [[ "$status" != "0" && "$status" != "124" ]]; then
    cat "$LOG"
    exit "$status"
fi

require_marker() {
    local marker="${1:?missing marker}"
    if ! grep -q "$marker" "$LOG"; then
        cat "$LOG"
        echo "RADPx-OS Pi Zero 2 W payload smoke missing marker: $marker" >&2
        exit 1
    fi
}

# Gate against the ordered parity marker set (kept in expected-markers.txt next to
# the board sources). The bcm283x Pi Zero 2 W boots the full stack under QEMU
# raspi3ap: SoC HAL bring-up, the microSD via the real-HW SDHOST controller + MBR partitions, the ext4 rootfs
# off the SD (RAD_SERVICE_ROOTFS_OK), the shared A53 kernel (MMU/exception/EL0/
# process-arch), the x86<->a53 parity self-tests (kernel-infra, in-guest UDP+TCP
# L4, named services), preemption (QA7 local-timer + EL1 CNTP: RAD_TIMER_IRQ_OK /
# RAD_PREEMPT_SCHED_OK), and the full rootfs userland (radinit -> services ->
# boot-session/rash: RAD_RADINIT_*, RAD_RASH_*, fork/COW/exec/wait/reap markers).
GATE="$ROOT/tools/embedded/rad_pi_zero2w/expected-markers.txt"
while IFS= read -r marker; do
    [[ -z "$marker" || "$marker" == \#* ]] && continue
    require_marker "$marker"
done < "$GATE"

# RAD_LOGIN_OK is exercised by the interactive login smoke (rad_pi_zero2w_login_smoke.sh),
# which types credentials -- this non-interactive marker gate stops at RAD_LOGIN_SPAWN_OK.
#
# DEFERRED -- genuinely absent hardware on QEMU raspi3ap, not a bug:
#   RAD_NET_HOST_UDP_ECHO_OK / RAD_NTP_* -- need a hardware NIC + SLIRP host
#     responder; raspi3ap has no GEM-equivalent (in-guest UDP/TCP L4 loopback is
#     covered above and is NIC-independent). SD writes are also not modeled
#     (read-only card), so RAD_FAT_RW_OK is not gated here.

echo "RADPx-OS Pi Zero 2 W payload smoke passed ($(grep -cvE '^\s*(#|$)' "$GATE") markers): $LOG"
