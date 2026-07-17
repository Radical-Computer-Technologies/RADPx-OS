#!/usr/bin/env bash
# ZuBoard-1CG QEMU marker smoke: runs run-qemu.sh, captures the serial log,
# and asserts the marker set from expected-markers.txt.
#
# Markers are checked in order (each must appear at or after the previous),
# EXCEPT lines prefixed with '~', which are presence-only (checked anywhere in
# the log without advancing the ordered cursor). Use '~' for markers emitted by
# a second core: their interleaving with core-0 markers is nondeterministic, so
# a strict total order across cores is not a valid assertion.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
MARKERS_FILE="${RADIX_ZUBOARD_MARKERS:-$SCRIPT_DIR/expected-markers.txt}"
LOG_FILE="${RADIX_ZUBOARD_SERIAL_LOG:-${TMPDIR:-/tmp}/radix-zuboard-qemu-serial.log}"

# Never run forever in CI; reuse the RadBuild-built ELF and QEMU SD image.
# Default 90s: -smp 2 under single-thread TCG runs ~2x slower per core.
export RADIX_QEMU_TIMEOUT="${RADIX_QEMU_TIMEOUT:-90}"
export RADIX_QEMU_BUILD="${RADIX_QEMU_BUILD:-0}"
export RADIX_ZUBOARD_IMAGE="${RADIX_ZUBOARD_IMAGE:-$REPO_ROOT/artifacts/radix/zuboard-1cg-qemu/radix-zuboard-1cg-qemu.img}"

if [ ! -f "$MARKERS_FILE" ]; then
    echo "qemu_marker_smoke: markers file not found: $MARKERS_FILE" >&2
    exit 1
fi

mkdir -p "$(dirname "$LOG_FILE")"

boot_and_check() {
    set +e
    "$SCRIPT_DIR/run-qemu.sh" </dev/null 2>&1 | tee "$LOG_FILE"
    local qemu_rc=${PIPESTATUS[0]}
    set -e
    # 124 = killed by timeout while idling at the serial prompt: expected.
    if [ "$qemu_rc" -ne 0 ] && [ "$qemu_rc" -ne 124 ]; then
        echo "qemu_marker_smoke: unexpected qemu exit status $qemu_rc" >&2
        return 2
    fi
    local fail=0
    local offset=0
    local marker hit
    while IFS= read -r marker; do
        case "$marker" in ''|'#'*) continue ;; esac
        if [ "${marker#\~}" != "$marker" ]; then
            # Presence-only: check anywhere, do not move the ordered cursor.
            # Used for cross-core markers and child-process output whose
            # interleaving with the main sequence is scheduling-dependent.
            marker="${marker#\~}"
            if grep -F -a -q -- "$marker" "$LOG_FILE"; then
                echo "ok (present): $marker"
            else
                echo "MISSING: $marker" >&2
                fail=1
            fi
            continue
        fi
        hit="$(tail -c +$((offset + 1)) "$LOG_FILE" \
            | grep -F -a -b -m1 -o -- "$marker" | head -n1 | cut -d: -f1 || true)"
        if [ -z "$hit" ]; then
            echo "MISSING/OUT-OF-ORDER: $marker" >&2
            fail=1
        else
            offset=$((offset + hit + ${#marker}))
            echo "ok: $marker"
        fi
    done < "$MARKERS_FILE"
    return "$fail"
}

# One retry: an intermittent QEMU-under-host-load anomaly (~1 in 5 on a busy
# machine) suppresses the kernel printk path for an entire boot -- the serial
# log collapses to ~50 lines while tty-path output still flows. A genuine
# regression fails both attempts. Tracked for a focused investigation; see the
# saved bimodal logs referenced in the Stage 2b/3a commit messages.
if boot_and_check; then
    echo "qemu_marker_smoke: all markers present in order"
    exit 0
fi
echo "qemu_marker_smoke: first attempt failed; retrying once (host-load boot anomaly)" >&2
if boot_and_check; then
    echo "qemu_marker_smoke: all markers present in order (second attempt)"
    exit 0
fi
echo "qemu_marker_smoke: FAILED on both attempts (serial log: $LOG_FILE)" >&2
exit 1
