#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PAYLOAD_DIR="$ROOT/tools/embedded/radix_pi_zero2w"
LOG="$ROOT/.radbuild/radix-pi-zero2w-payload-smoke.log"

# shellcheck source=/dev/null
source "$ROOT/tools/embedded/qemu_aarch64_env.sh"
QEMU="$(radix_resolve_qemu_system_aarch64 "$ROOT")"

make -C "$PAYLOAD_DIR" clean >/dev/null 2>&1 || true
make -C "$PAYLOAD_DIR" >/dev/null

mkdir -p "$(dirname "$LOG")"
set +e
timeout "${RADIX_PI_QEMU_TIMEOUT:-8s}" "$QEMU" \
    -M raspi3b \
    -cpu cortex-a53 \
    -m 1G \
    -kernel "$PAYLOAD_DIR/RADIXKRN.IMG" \
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
        echo "RADix Pi Zero 2 W payload smoke missing marker: $marker" >&2
        exit 1
    fi
}

require_marker "RADIX_PI_BCM283X_HAL_OK"
require_marker "RADIX_PI_UART_OK"
require_marker "RADIX_PI_MAILBOX_FB_OK"
require_marker "RADIX_PI_HANDOFF_OK"
require_marker "RADIX_PI_PAYLOAD_ENTRY_OK"
require_marker "RADIX_PI_FRAMEBUFFER_TEXT_OK"

echo "RADix Pi Zero 2 W payload smoke passed: $LOG"
