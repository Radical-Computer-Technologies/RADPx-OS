#!/usr/bin/env bash
#
# Interactive login smoke for the Raspberry Pi Zero 2 W: boots the ext4 rootfs off
# the SD card, waits for the login session, types the default credentials
# (root/rad), and confirms RAD_LOGIN_OK. Complements rad_pi_zero2w_smoke.sh (the
# non-interactive marker gate, which stops at RAD_LOGIN_SPAWN_OK).
#
# NOTE: on the Pi, raw TTY output (login/Password prompts) is not surfaced on the
# UART -- only kernel debug markers and marker-shaped userland lines are (the WRITE
# syscall path scans them). So this drives blind: it polls the serial log for
# readiness markers, sends credentials, then waits for RAD_LOGIN_OK (a marker-shaped
# line login writes on success).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PAYLOAD_DIR="$ROOT/tools/embedded/rad_pi_zero2w"
LOG="${RAD_PI_LOGIN_LOG:-$ROOT/.radbuild/rad-pi-zero2w-login-smoke.log}"
LOGIN_USER="${RAD_PI_LOGIN_USER:-root}"
LOGIN_PASSWORD="${RAD_PI_LOGIN_PASSWORD:-rad}"
TIMEOUT="${RAD_PI_LOGIN_TIMEOUT:-30}"

# shellcheck source=/dev/null
source "$ROOT/tools/embedded/qemu_aarch64_env.sh"
QEMU="$(rad_resolve_qemu_system_aarch64 "$ROOT")"

make -C "$PAYLOAD_DIR" >/dev/null
SD_IMG="$ROOT/artifacts/rad/pi-zero2w/pi-zero2w-sd.img"
bash "$ROOT/tools/embedded/rad_pi_zero2w_mkimage.sh" >/dev/null

mkdir -p "$(dirname "$LOG")"
: > "$LOG"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/rad-pi-login.XXXXXX")"
FIFO="$WORK/in"
mkfifo "$FIFO"
trap 'rm -rf "$WORK"; [[ -n "${QPID:-}" ]] && kill "$QPID" 2>/dev/null || true' EXIT

# Launch QEMU first (it opens the FIFO read end), then open the write end on fd 3
# so QEMU's stdin never sees EOF. Opening the two ends in the other order
# deadlocks (a FIFO open blocks until the opposite end is opened).
timeout "$TIMEOUT" "$QEMU" -M raspi3ap -cpu cortex-a53 -m 512M \
    -kernel "$PAYLOAD_DIR/RADKRN.IMG" \
    -drive "file=$SD_IMG,if=sd,format=raw" \
    -serial stdio -display none -no-reboot < "$FIFO" > "$LOG" 2>&1 &
QPID=$!
exec 3>"$FIFO"

await() {
    local pattern="$1" deadline=$(( SECONDS + ${2:-20} ))
    while (( SECONDS < deadline )); do
        grep -q "$pattern" "$LOG" && return 0
        kill -0 "$QPID" 2>/dev/null || { grep -q "$pattern" "$LOG" && return 0; return 1; }
        sleep 0.25
    done
    return 1
}
send() { printf '%s\r' "$1" >&3; }

# Wait for the rootfs login session to be spawned, then drive credentials blind.
await "RAD_LOGIN_SPAWN_OK" 25 || { echo "pi_login_smoke: login session never spawned" >&2; cat "$LOG"; exit 1; }
sleep 1
send "$LOGIN_USER"
sleep 1
send "$LOGIN_PASSWORD"

if await "RAD_LOGIN_OK" 15; then
    echo "rad_pi_zero2w_login_smoke: RAD_LOGIN_OK (login accepted) -- passed (log: $LOG)"
    exit 0
fi
echo "rad_pi_zero2w_login_smoke: FAILED -- RAD_LOGIN_OK not seen (log: $LOG)" >&2
exit 1
