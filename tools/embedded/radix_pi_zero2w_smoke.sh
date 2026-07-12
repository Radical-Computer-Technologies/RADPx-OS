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
require_marker "RADIX_PI_EMMC_INIT_OK"
require_marker "RADIX_PI_MMCBLK0_OK"
require_marker "RADIX_PI_BLOCK_READ_OK"
require_marker "RADIX_PI_FAT_MOUNT_OK"
require_marker "RADIX_PI_USB_CORE_OK"
require_marker "RADIX_PI_USB_HID_KEYBOARD_OK"
require_marker "RADIX_PI_USB_HID_MOUSE_OK"
require_marker "RADIX_PI_HANDOFF_OK"
require_marker "RADIX_PI_PAYLOAD_ENTRY_OK"
require_marker "RADIX_PI_SECONDARIES_PARKED_OK"
require_marker "RADIX_PI_CLEAN_CPU_STATE_OK"
require_marker "RADIX_PI_FRAMEBUFFER_TEXT_OK"
require_marker "RADIX_PI_FRAMEBUFFER_DIRTY_PRESENT_OK"
require_marker "RADIX_AARCH64_EL0_OK"
require_marker "RADIX_AARCH64_SVC_OK"
require_marker "RADIX_AARCH64_USER_COPY_OK"
require_marker "RADIX_AARCH64_EXECVE_OK"
require_marker "RADIX_AARCH64_FORK_OK"
require_marker "RADIX_AARCH64_COW_PAGE_FAULT_OK"
require_marker "RADIX_AARCH64_COW_PARENT_ISOLATED_OK"
require_marker "RADIX_PI_SLINT_BOOT_SHELL_OK"
require_marker "RADIX_PI_SLINT_WM_OK"
require_marker "RADIX_PI_SLINT_APP_TERMINAL_WINDOW_OK"
require_marker "RADIX_PI_COMPOSITOR_DAMAGE_QUEUE_OK"

echo "RADix Pi Zero 2 W payload smoke passed: $LOG"
