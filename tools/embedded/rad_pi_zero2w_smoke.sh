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
# (kernel-infra, in-guest UDP+TCP L4, named services, base-terminal), init spawn,
# and the framebuffer/compositor path.
GATE="$ROOT/tools/embedded/rad_pi_zero2w/expected-markers.txt"
while IFS= read -r marker; do
    [[ -z "$marker" || "$marker" == \#* ]] && continue
    require_marker "$marker"
done < "$GATE"

# DEFERRED -- needs bcm283x preemption (QA7 local-timer + CNTP scheduler tick, the
# Pi analogue of the ZuBoard's rad_zynqmp_preempt_init GICv2+CNTP path). Without a
# preemptive tick the cooperative poll loop cannot drive the userland fork/exec/
# wait smoke to completion (init reaches EL0 and its first syscall, then radsh's
# fork->waitpid cannot schedule its child), and there is no live interactive shell.
# These markers gate on the ZuBoard today and will gate here once preemption lands:
#   RAD_AARCH64_USER_INIT_OK RAD_AARCH64_USER_SYSCALLS_OK
#   RAD_AARCH64_USER_EXECVE_OK RAD_AARCH64_USER_RADSH_BOOT_OK
#   RAD_AARCH64_USER_FORK_OK RAD_AARCH64_USER_FORK_CHILD_OK
#   RAD_AARCH64_USER_FORK_WAIT_OK RAD_AARCH64_USER_ZOMBIE_REAP_OK
#   RAD_AARCH64_USER_SH_SCRIPT_OK RAD_AARCH64_USERMODE_EXIT_OK
#   RAD_AARCH64_FORK_OK RAD_AARCH64_COW_PAGE_FAULT_OK RAD_LOGIN_OK

echo "RADPx-OS Pi Zero 2 W payload smoke passed ($(grep -cvE '^\s*(#|$)' "$GATE") markers): $LOG"
