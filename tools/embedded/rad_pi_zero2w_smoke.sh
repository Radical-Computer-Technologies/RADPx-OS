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

mkdir -p "$(dirname "$LOG")"
set +e
timeout "${RAD_PI_QEMU_TIMEOUT:-8s}" "$QEMU" \
    -M raspi3b \
    -cpu cortex-a53 \
    -m 1G \
    -kernel "$PAYLOAD_DIR/RADKRN.IMG" \
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
# the board sources). This is the set the bcm283x Pi Zero 2 W boots reliably from a
# kernel-only image under QEMU raspi3b: the SoC HAL bring-up, the shared A53
# kernel (MMU/exception/EL0/process-arch), the x86<->a53 parity self-tests
# (kernel-infra, in-guest UDP+TCP L4, named services, base-terminal), preemption
# (QA7 local-timer + EL1 CNTP: RAD_TIMER_IRQ_OK / RAD_PREEMPT_SCHED_OK), the full
# preemptive userland smoke (init -> radsh -> fork/COW/exec/wait/reap/shebang:
# RAD_AARCH64_FORK/USER_FORK/COW_PAGE_FAULT/USER_ZOMBIE_REAP/USER_EXECVE/
# USER_EXECVE_REENTER/USER_PIPE_FORK/USER_WAIT_WAKE/USER_SCRIPT_SHEBANG/
# USERMODE_EXIT_OK), and the framebuffer/compositor path.
GATE="$ROOT/tools/embedded/rad_pi_zero2w/expected-markers.txt"
while IFS= read -r marker; do
    [[ -z "$marker" || "$marker" == \#* ]] && continue
    require_marker "$marker"
done < "$GATE"

# DEFERRED -- these are userland-PRINTED strings (init.S/radsh.S write them to
# stdout), not kernel markers. They gate on the x86 image because its userland
# stdout reaches the serial log; on the Pi the /dev/console device-write path does
# not yet surface on the UART (kernel rad_debug_marker output does), so init/radsh
# console text -- and a visible interactive shell (RAD_LOGIN_OK) -- do not appear.
# Wiring the /dev/console device writes to the PL011 UART unlocks:
#   RAD_AARCH64_USER_INIT_OK RAD_AARCH64_USER_SYSCALLS_OK
#   RAD_AARCH64_USER_INVALID_PTR_OK RAD_AARCH64_USER_RADSH_BOOT_OK
#   RAD_AARCH64_USER_FORK_CHILD_OK RAD_AARCH64_USER_FORK_WAIT_OK
#   RAD_AARCH64_USER_SH_SCRIPT_OK RAD_AARCH64_USER_RADSH_EXIT_OK RAD_LOGIN_OK

echo "RADPx-OS Pi Zero 2 W payload smoke passed ($(grep -cvE '^\s*(#|$)' "$GATE") markers): $LOG"
