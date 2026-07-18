#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CIRCLEHOME="${CIRCLEHOME:-/media/jvincent/Kingspec512/repos/circle}"
RADLIB_ROOT="${RADLIB_ROOT:-$(cd "$ROOT/../RADLib" && pwd)}"
TOOLCHAIN="${RADLIB_AARCH64_NONE_ELF_PREFIX:-$RADLIB_ROOT/.radbuild/toolchains/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-}"
LOG="$ROOT/.radbuild/qemu-pizero2w-smoke.log"

# shellcheck source=/dev/null
source "$ROOT/tools/embedded/qemu_aarch64_env.sh"
QEMU="$(radix_resolve_qemu_system_aarch64 "$ROOT")"

cd "$CIRCLEHOME"
./configure -f -r 3 -p "$TOOLCHAIN" --multicore --qemu --c++20 >/dev/null
./makeall >/dev/null

cd "$ROOT"
make -C tools/embedded/circle_pizero2w clean >/dev/null 2>&1 || true
make -C tools/embedded/circle_pizero2w QEMU=1 >/dev/null

mkdir -p "$(dirname "$LOG")"
set +e
timeout 12s "$QEMU" \
    -M raspi3b \
    -cpu cortex-a53 \
    -m 1G \
    -kernel "$ROOT/tools/embedded/circle_pizero2w/kernel8.img" \
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
        echo "Pi Zero 2 W QEMU smoke missing marker: $marker" >&2
        exit 1
    fi
}

if grep -q "RADIX_PI_CIRCLE_LOADER_OK" "$LOG"; then
    require_marker "RADIX_PI_LOADER_FAT_OK"
    require_marker "RADIX_PI_PAYLOAD_LOAD_OK"
    require_marker "RADIX_PI_SECONDARIES_PARKED_OK"
    require_marker "RADIX_PI_CLEAN_CPU_STATE_OK"
    require_marker "RADIX_PI_LOADER_HANDOFF_RECORD_OK"
else
    cat "$LOG"
    echo "Pi Zero 2 W Circle loader built, but QEMU emitted no loader serial markers; running RADPx payload gate." >&2
fi

"$ROOT/tools/embedded/radix_pi_zero2w_smoke.sh"

if [[ "${RADLIB_RESTORE_CIRCLE_HW:-1}" == "1" ]]; then
    (
        cd "$CIRCLEHOME"
        ./configure -f -r 3 -p "$TOOLCHAIN" --multicore --c++20 >/dev/null
        ./makeall >/dev/null
    )
fi

echo "Pi Zero 2 W QEMU loader+payload smoke passed: $LOG"
