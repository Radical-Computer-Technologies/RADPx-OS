#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
jobs="${RADLIB_BUILD_JOBS:-4}"

build_one() {
    local name="$1"
    local build_dir="$2"
    local autotest="$3"
    local autologin="$4"
    local nano="${5:-false}"
    local nano_variant="${6:-none}"
    local max_user_processes="${7:-128}"
    local artifact_dir="${repo_root}/artifacts/radix/x86_64-grub-terminal-${name}"

    cmake -S "${repo_root}/tools/embedded/x86_64_grub_slint" -B "${build_dir}" \
        -DRADIX_X86_UI_PROFILE=terminal \
        -DRADIX_X86_TERMINAL_AUTOTEST_NANO="${autotest}" \
        -DRADIX_X86_TERMINAL_AUTOLOGIN="${autologin}" \
        -DRADIX_RKCONFIG_TERMINAL_NANO="${nano}" \
        -DRADIX_RKCONFIG_TERMINAL_NANO_VARIANT="${nano_variant}" \
        -DRADIX_X86_MAX_USER_PROCESSES="${max_user_processes}" \
        -DRADIX_RKCONFIG_KERNEL_MAX_TASKS="${RADIX_RKCONFIG_KERNEL_MAX_TASKS:-128}" \
        -DRADIX_RKCONFIG_KERNEL_MAX_PROCESSES="${RADIX_RKCONFIG_KERNEL_MAX_PROCESSES:-128}" \
        -DRADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES="${RADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES:-524288}" \
        -DRADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY="${RADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY:-dynamic}"
    cmake --build "${build_dir}" --target radixkernel_x86_64_grub_slint_iso -j"${jobs}"

    mkdir -p "${artifact_dir}"
    cp "${build_dir}/radixkernel-x86-64-grub-terminal.iso" "${artifact_dir}/radixkernel-x86-64-grub-terminal-${name}.iso"
    cp "${build_dir}/radix-rootfs.ext4" "${artifact_dir}/radix-rootfs-${name}.ext4"
    cp "${build_dir}/radix-fat32.img" "${artifact_dir}/radix-fat32-${name}.img"

    echo "${name}:"
    echo "  ISO:    ${build_dir}/radixkernel-x86-64-grub-terminal.iso"
    echo "  rootfs: ${build_dir}/radix-rootfs.ext4"
    echo "  FAT32:  ${build_dir}/radix-fat32.img"
    echo "  artifacts: ${artifact_dir}"
}

build_one \
    "interactive" \
    "${repo_root}/build/embedded/x86_64_grub_terminal_interactive" \
    false \
    false \
    false \
    none \
    128

build_one \
    "smoke" \
    "${repo_root}/build/embedded/x86_64_grub_terminal_smoke" \
    true \
    true \
    false \
    none \
    128
