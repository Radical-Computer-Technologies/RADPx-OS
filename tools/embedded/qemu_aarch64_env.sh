#!/usr/bin/env bash

radix_resolve_qemu_system_aarch64() {
    local root="${1:?missing repo root}"
    local qemu="${QEMU_SYSTEM_AARCH64:-}"

    if [[ -z "$qemu" ]]; then
        if command -v qemu-system-aarch64 >/dev/null 2>&1; then
            qemu="$(command -v qemu-system-aarch64)"
        else
            qemu="$root/.radbuild/qemu-arm/usr/bin/qemu-system-aarch64"
        fi
    fi

    if [[ ! -x "$qemu" ]]; then
        mkdir -p "$root/.radbuild/qemu-arm-deb" "$root/.radbuild/qemu-arm"
        (
            cd "$root/.radbuild/qemu-arm-deb"
            rm -f qemu-system-arm_*.deb
            apt-get download qemu-system-arm >&2
            debs=(qemu-system-arm_*.deb)
            dpkg-deb -x "${debs[0]}" "$root/.radbuild/qemu-arm" >&2
        )
    fi

    if [[ ! -x "$qemu" ]]; then
        echo "qemu-system-aarch64 not found; install qemu-system-arm or set QEMU_SYSTEM_AARCH64" >&2
        return 1
    fi

    printf '%s\n' "$qemu"
}
