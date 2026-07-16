#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
QEMU_SYSTEM_AARCH64="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"
ARTIFACT_IMAGE="${RADIX_ZUBOARD_IMAGE:-$REPO_ROOT/artifacts/radix/zuboard-1cg-serial/radix-zuboard-1cg.img}"
QEMU_IMAGE="${RADIX_ZUBOARD_QEMU_IMAGE:-/tmp/radix-zuboard-1cg-qemu.img}"
MEMORY="${RADIX_QEMU_MEMORY:-1024M}"
SMP="${RADIX_QEMU_SMP:-1}"
TIMEOUT_SECONDS="${RADIX_QEMU_TIMEOUT:-0}"

if [[ "${RADIX_QEMU_BUILD:-1}" != "0" ]]; then
    make -C "$SCRIPT_DIR" clean all RADIX_ZYNQMP_SDHCI_BASE=0xff160000u
fi

if [[ ! -f "$ARTIFACT_IMAGE" ]]; then
    echo "missing SD image: $ARTIFACT_IMAGE" >&2
    echo "run radbuild for the zuboard-1cg-serial target first, or set RADIX_ZUBOARD_IMAGE" >&2
    exit 1
fi

cp "$ARTIFACT_IMAGE" "$QEMU_IMAGE"
truncate -s 512M "$QEMU_IMAGE"

cmd=(
    "$QEMU_SYSTEM_AARCH64"
    -accel tcg,thread=single
    -M xlnx-zcu102
    -m "$MEMORY"
    -smp "$SMP"
    -display none
    -monitor none
    -serial stdio
    -drive "if=sd,format=raw,file=$QEMU_IMAGE"
    -device "loader,file=$SCRIPT_DIR/radix-zuboard.elf,cpu-num=0"
)

if [[ "$TIMEOUT_SECONDS" != "0" ]]; then
    exec timeout "$TIMEOUT_SECONDS" "${cmd[@]}"
fi

exec "${cmd[@]}"
