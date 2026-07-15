#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
ui_profile="${RADIX_X86_UI_PROFILE:-wm}"
case "${ui_profile}" in
    wm|terminal) ;;
    *)
        echo "RADIX_X86_UI_PROFILE must be wm or terminal" >&2
        exit 2
        ;;
esac
default_build_dir="${repo_root}/build/embedded/x86_64_grub_${ui_profile}"
build_dir="${RADLIB_X86_64_GRUB_SLINT_BUILD_DIR:-${default_build_dir}}"
display="${RADLIB_X86_QEMU_DISPLAY:-gtk}"
build=1
virtio_disk="${RADLIB_X86_VIRTIO_DISK:-}"
virtio_fat="${RADLIB_X86_VIRTIO_FAT:-}"
enable_net="${RADLIB_X86_ENABLE_NET:-1}"
kernel_name="radixkernel-x86-64-grub-${ui_profile}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --headless)
            display="none"
            shift
            ;;
        --display)
            display="$2"
            shift 2
            ;;
        --no-build)
            build=0
            shift
            ;;
        --virtio-disk)
            virtio_disk="$2"
            shift 2
            ;;
        --no-net)
            enable_net=0
            shift
            ;;
        *)
            echo "usage: $0 [--headless] [--display gtk|sdl|none] [--virtio-disk path] [--no-net] [--no-build]" >&2
            exit 2
            ;;
    esac
done

if [[ "${build}" == "1" ]]; then
    cmake -S "${repo_root}/tools/embedded/x86_64_grub_slint" -B "${build_dir}" \
        -DRADIX_X86_UI_PROFILE="${ui_profile}" \
        -DRADIX_X86_TERMINAL_AUTOTEST_NANO="${RADIX_X86_TERMINAL_AUTOTEST_NANO:-false}" \
        -DRADIX_X86_TERMINAL_AUTOLOGIN="${RADIX_X86_TERMINAL_AUTOLOGIN:-false}" \
        -DRADIX_RKCONFIG_TERMINAL_NANO="${RADIX_RKCONFIG_TERMINAL_NANO:-false}" \
        -DRADIX_RKCONFIG_TERMINAL_NANO_VARIANT="${RADIX_RKCONFIG_TERMINAL_NANO_VARIANT:-none}" \
        -DRADIX_RKCONFIG_TERMINAL_VIM="${RADIX_RKCONFIG_TERMINAL_VIM:-true}" \
        -DRADIX_RKCONFIG_TERMINAL_VIM_VARIANT="${RADIX_RKCONFIG_TERMINAL_VIM_VARIANT:-tiny}" \
        -DRADIX_X86_MAX_USER_PROCESSES="${RADIX_X86_MAX_USER_PROCESSES:-128}" \
        -DRADIX_RKCONFIG_KERNEL_MAX_TASKS="${RADIX_RKCONFIG_KERNEL_MAX_TASKS:-128}" \
        -DRADIX_RKCONFIG_KERNEL_MAX_PROCESSES="${RADIX_RKCONFIG_KERNEL_MAX_PROCESSES:-128}" \
        -DRADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES="${RADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES:-524288}" \
        -DRADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY="${RADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY:-dynamic}"
    cmake --build "${build_dir}" --target radixkernel_x86_64_grub_slint_iso -j"${RADLIB_BUILD_JOBS:-2}"
fi

if [[ -z "${virtio_disk}" && -f "${build_dir}/radix-rootfs.ext4" ]]; then
    virtio_disk="${build_dir}/radix-rootfs.ext4"
fi
if [[ -z "${virtio_fat}" && -f "${build_dir}/radix-fat32.img" ]]; then
    virtio_fat="${build_dir}/radix-fat32.img"
fi

qemu_args=(
    -m "${RADLIB_X86_QEMU_MEMORY:-512M}"
    -smp "${RADLIB_X86_QEMU_SMP:-4}"
    -vga std
    -cdrom "${build_dir}/${kernel_name}.iso"
    -serial stdio
    -display "${display}"
    -no-reboot
)

if [[ -n "${virtio_disk}" ]]; then
    qemu_args+=(
        -drive "if=none,id=radixdisk,format=raw,file=${virtio_disk}"
        -device "virtio-blk-pci,drive=radixdisk,disable-modern=on"
    )
fi
if [[ -n "${virtio_fat}" ]]; then
    qemu_args+=(
        -drive "if=none,id=radixfat,format=raw,file=${virtio_fat}"
        -device "virtio-blk-pci,drive=radixfat,disable-modern=on"
    )
fi

if [[ "${enable_net}" != "0" ]]; then
    responder_log="${build_dir}/radix-host-net-responder.log"
    rm -f "${responder_log}"
    python3 "${script_dir}/x86_64_grub_slint/host_udp_ntp_responder.py" \
        --ntp-port "${RADIX_HOST_NTP_PORT:-12300}" \
        --echo-port "${RADIX_HOST_UDP_ECHO_PORT:-12301}" \
        --duration "${RADLIB_X86_HOST_NET_RESPONDER_DURATION:-86400}" \
        >"${responder_log}" 2>&1 &
    responder_pid=$!
    trap 'kill "${responder_pid}" >/dev/null 2>&1 || true' EXIT
    qemu_args+=(
        -netdev "user,id=radixnet"
        -device "virtio-net-pci,netdev=radixnet,disable-modern=on"
    )
    qemu-system-x86_64 "${qemu_args[@]}"
    exit $?
fi

exec qemu-system-x86_64 "${qemu_args[@]}"
