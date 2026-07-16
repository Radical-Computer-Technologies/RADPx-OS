#!/usr/bin/env bash
# ZuBoard-1CG interactive login smoke: drives the QEMU serial console through
# login (root/radix) into rash and exercises ls/cat/ps. Complements
# qemu_marker_smoke.sh (non-interactive marker gate).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
LOG_FILE="${RADIX_ZUBOARD_LOGIN_LOG:-${TMPDIR:-/tmp}/radix-zuboard-login-smoke.log}"
LOGIN_USER="${RADIX_ZUBOARD_LOGIN_USER:-root}"
LOGIN_PASSWORD="${RADIX_ZUBOARD_LOGIN_PASSWORD:-radix}"
BOOT_TIMEOUT="${RADIX_ZUBOARD_LOGIN_BOOT_TIMEOUT:-75}"
STEP_TIMEOUT="${RADIX_ZUBOARD_LOGIN_STEP_TIMEOUT:-15}"

export RADIX_QEMU_TIMEOUT="${RADIX_QEMU_TIMEOUT:-120}"
export RADIX_QEMU_BUILD="${RADIX_QEMU_BUILD:-0}"
export RADIX_ZUBOARD_IMAGE="${RADIX_ZUBOARD_IMAGE:-$REPO_ROOT/artifacts/radix/zuboard-1cg-qemu/radix-zuboard-1cg-qemu.img}"

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/radix-login-smoke.XXXXXX")"
FIFO="$WORK_DIR/serial-in"
mkfifo "$FIFO"
mkdir -p "$(dirname "$LOG_FILE")"
: > "$LOG_FILE"

cleanup() {
    exec 3>&- 2>/dev/null || true
    [ -n "${QEMU_PID:-}" ] && kill "$QEMU_PID" 2>/dev/null || true
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

"$SCRIPT_DIR/run-qemu.sh" < "$FIFO" > "$LOG_FILE" 2>&1 &
QEMU_PID=$!
exec 3>"$FIFO"

await() {
    local pattern="$1"
    local budget="$2"
    local deadline=$((SECONDS + budget))
    while [ "$SECONDS" -lt "$deadline" ]; do
        if grep -q -a -F -- "$pattern" "$LOG_FILE"; then
            return 0
        fi
        if ! kill -0 "$QEMU_PID" 2>/dev/null; then
            echo "qemu_login_smoke: QEMU exited while waiting for: $pattern" >&2
            return 1
        fi
        sleep 0.25
    done
    echo "qemu_login_smoke: timeout waiting for: $pattern" >&2
    return 1
}

send() {
    printf '%s\r' "$1" >&3
}

fail() {
    echo "qemu_login_smoke: FAILED at: $1 (log: $LOG_FILE)" >&2
    exit 1
}

await "login:" "$BOOT_TIMEOUT"            || fail "login prompt"
send "$LOGIN_USER"
await "Password:" "$STEP_TIMEOUT"         || fail "password prompt"
send "$LOGIN_PASSWORD"
await "RADIX_LOGIN_OK" "$STEP_TIMEOUT"    || fail "login accepted"
send "ls /"
await "bin" "$STEP_TIMEOUT"               || fail "ls output"
send "cat /etc/hostname"
await "radix" "$STEP_TIMEOUT"             || fail "cat output"
send "ps"
await "PID" "$STEP_TIMEOUT"               || fail "ps output"
send "exit"

echo "qemu_login_smoke: interactive session passed (log: $LOG_FILE)"
