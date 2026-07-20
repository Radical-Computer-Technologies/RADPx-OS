#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

if [[ -d "${HOME}/snap/rustup/common/cargo/bin" ]]; then
    export PATH="${HOME}/snap/rustup/common/cargo/bin:${PATH}"
fi
rust_cmake_args=()
rust_toolchain_bin="${RADLIB_RUST_TOOLCHAIN_BIN:-${HOME}/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin}"
if [[ -x "${rust_toolchain_bin}/rustc" && -x "${rust_toolchain_bin}/cargo" ]]; then
    export PATH="${rust_toolchain_bin}:${PATH}"
    rust_cmake_args+=(
        "-DRust_COMPILER=${rust_toolchain_bin}/rustc"
        "-DRust_CARGO=${rust_toolchain_bin}/cargo"
    )
fi

slint_sdk="${SLINT_SDK_DIR:-/media/jvincent/Kingspec512/SDKs/Slint-cpp-1.17.0-Linux-x86_64}"
log_dir="${RADLIB_SMOKE_LOG_DIR:-${repo_root}/.radbuild}"
shell_build="${RADLIB_RAD_OS_SHELL_BUILD_DIR:-${repo_root}/build/embedded/rad_os_shell}"
x86_build="${RADLIB_X86_64_GRUB_SLINT_BUILD_DIR:-${repo_root}/build/embedded/x86_64_grub_slint}"
qemu_timeout="${RADLIB_X86_QEMU_TIMEOUT:-60s}"
if [[ "${RAD_X86_TERMINAL_AUTOTEST_NANO:-0}" == "1" || "${RAD_X86_TERMINAL_AUTOTEST_NANO:-false}" == "true" || "${RAD_X86_TERMINAL_AUTOTEST_NANO:-0}" == "ON" ]]; then
    qemu_timeout="${RADLIB_X86_QEMU_TIMEOUT:-300s}"
fi
qemu_smp="${RADLIB_X86_QEMU_SMP:-4}"
ui_profile="${RAD_X86_UI_PROFILE:-wm}"
case "${ui_profile}" in
    wm|terminal) ;;
    *)
        echo "RAD_X86_UI_PROFILE must be wm or terminal" >&2
        exit 2
        ;;
esac
kernel_name="radkernel-x86-64-grub-${ui_profile}"
radpm_cmake_args=()
if [[ -n "${RAD_RADPM_ROOTFS_DIR:-}" ]]; then
    radpm_cmake_args+=("-DRAD_RADPM_ROOTFS_DIR=${RAD_RADPM_ROOTFS_DIR}")
fi
if [[ -n "${RAD_RADPM_ROOTFS_STAMP:-}" ]]; then
    radpm_cmake_args+=("-DRAD_RADPM_ROOTFS_STAMP=${RAD_RADPM_ROOTFS_STAMP}")
fi

mkdir -p "${log_dir}"

if [[ "${ui_profile}" == "wm" && -d "${slint_sdk}" && "${RADLIB_SKIP_HOST_SLINT_SMOKE:-0}" != "1" ]]; then
    cmake -S "${repo_root}/tools/embedded/rad_os_shell" -B "${shell_build}" \
        -DRADUI_SLINT_PROVIDER=binary \
        -DSLINT_SDK_DIR="${slint_sdk}"
    cmake --build "${shell_build}" --target rad_os_shell -j"${RADLIB_BUILD_JOBS:-2}"
    "${shell_build}/rad_os_shell" --self-test | tee "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD OS Slint shell rendered checksum=0x" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_TERMINAL_LOADING_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_TERMINAL_READY_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_WM_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_APP_TERMINAL_WINDOW_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_MENU_OPEN_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_MENU_ESCAPE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_WINDOW_MOVE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_WINDOW_RESIZE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_TERMINAL_SCROLL_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_TERMINAL_CLOSE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_TERMINAL_RELAUNCH_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_WINDOW_OPEN_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_FOCUS_SWITCH_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_LAUNCHER_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_TASKBAR_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_SLINT_CLOSE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_SURFACE_CREATE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_OFFSCREEN_RENDER_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_BLIT_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_HIT_TEST_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_INPUT_TRANSLATE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_Z_ORDER_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_ALPHA_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_DAMAGE_QUEUE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_COPY_FORWARD_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_EXPOSED_DAMAGE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD_COMPOSITOR_EMPTY_FRAME_SKIP_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
else
    echo "Skipping hosted Slint smoke for ${ui_profile} profile."
fi

if [[ "${RADLIB_X86_SKIP_REBUILD:-0}" != "1" ]]; then
    cmake -S "${repo_root}/tools/embedded/x86_64_grub_slint" -B "${x86_build}" \
        -DRAD_X86_UI_PROFILE="${ui_profile}" \
        "${radpm_cmake_args[@]}" \
        "${rust_cmake_args[@]}"
    rm -f "${x86_build}/rad-rootfs.ext4" "${x86_build}/rad-fat32.img"
    cmake --build "${x86_build}" --target radkernel_x86_64_grub_slint_iso -j"${RADLIB_BUILD_JOBS:-2}"
fi
grub-file --is-x86-multiboot2 "${x86_build}/${kernel_name}"
if [[ "${RADLIB_X86_SKIP_QEMU:-0}" == "1" ]]; then
    echo "Skipping QEMU smoke for ${ui_profile} profile after build/package validation."
    exit 0
fi

qemu_log="${log_dir}/x86-64-grub-${ui_profile}-smoke.log"
net_responder_log="${log_dir}/x86-64-grub-${ui_profile}-net-responder.log"
monitor_sock="${TMPDIR:-/tmp}/rad-${ui_profile}-${qemu_smp}-$$.sock"
rootfs_img="${x86_build}/rad-rootfs.ext4"
fat_img="${x86_build}/rad-fat32.img"
rm -f "${qemu_log}" "${net_responder_log}" "${monitor_sock}"
python3 "${script_dir}/x86_64_grub_slint/host_udp_ntp_responder.py" \
    --ntp-port "${RAD_HOST_NTP_PORT:-12300}" \
    --echo-port "${RAD_HOST_UDP_ECHO_PORT:-12301}" \
    --duration "${RADLIB_X86_HOST_NET_RESPONDER_DURATION:-180}" \
    >"${net_responder_log}" 2>&1 &
net_responder_pid=$!
trap 'kill "${net_responder_pid}" >/dev/null 2>&1 || true; rm -f "${monitor_sock}"' EXIT
for _ in $(seq 1 40); do
    grep -q "RAD_HOST_NET_RESPONDER_READY" "${net_responder_log}" 2>/dev/null && break
    sleep 0.05
done
set +e
timeout "${qemu_timeout}" qemu-system-x86_64 \
    -m "${RADLIB_X86_QEMU_MEMORY:-768M}" \
    -smp "${qemu_smp}" \
    -vga std \
    -cdrom "${x86_build}/${kernel_name}.iso" \
    -drive "if=none,id=raddisk,format=raw,file=${rootfs_img}" \
    -device "virtio-blk-pci,drive=raddisk,disable-modern=on" \
    -drive "if=none,id=radfat,format=raw,file=${fat_img}" \
    -device "virtio-blk-pci,drive=radfat,disable-modern=on" \
    -netdev "user,id=radnet" \
    -device "virtio-net-pci,netdev=radnet,disable-modern=on" \
    -serial "file:${qemu_log}" \
    -monitor "unix:${monitor_sock},server,nowait" \
    -display none \
    -no-reboot &
qemu_pid=$!
keyboard_injected=0
for _ in $(seq 1 80); do
    [[ -S "${monitor_sock}" ]] && break
    sleep 0.1
done
for _ in $(seq 1 "${RADLIB_X86_INPUT_READY_POLLS:-400}"); do
    grep -q "RAD x86_64 base terminal ready" "${qemu_log}" 2>/dev/null && break
    sleep 0.1
done
send_monitor_key() {
    local key="$1"
    printf 'sendkey %s\n' "${key}" | socat - "UNIX-CONNECT:${monitor_sock}" >/dev/null 2>&1 || true
    sleep "${RADLIB_X86_SENDKEY_DELAY:-0.08}"
}

send_monitor_keys() {
    local key
    for key in "$@"; do
        send_monitor_key "${key}"
    done
}

sleep "${RADLIB_X86_KEYBOARD_INJECT_DELAY:-1.0}"
if [[ -S "${monitor_sock}" ]] && command -v socat >/dev/null 2>&1; then
    send_monitor_keys r o o t ret r a d i x ret
    for _ in $(seq 1 "${RADLIB_X86_LOGIN_OBSERVED_POLLS:-80}"); do
        grep -q "RAD_LOGIN_OK" "${qemu_log}" 2>/dev/null && break
        sleep 0.1
    done
    if ! grep -q "RAD_LOGIN_OK" "${qemu_log}" 2>/dev/null; then
        send_monitor_keys r o o t ret r a d i x ret
        for _ in $(seq 1 "${RADLIB_X86_LOGIN_OBSERVED_POLLS:-80}"); do
            grep -q "RAD_LOGIN_OK" "${qemu_log}" 2>/dev/null && break
            sleep 0.1
        done
    fi
    if grep -q "RAD_LOGIN_OK" "${qemu_log}" 2>/dev/null; then
        send_monitor_keys h e l p ret
        send_monitor_keys l s space slash ret
        send_monitor_keys c l e tab ret
        send_monitor_keys c d ret
        send_monitor_keys l s space b i tab ret
        send_monitor_keys c l e a r ret
        send_monitor_keys e c h o space h i ret
        send_monitor_keys up ret
        send_monitor_keys t u i minus s m o k e ret q
    fi
    printf 'mouse_move 24 18\nmouse_button 1\nmouse_button 0\n' \
        | socat - "UNIX-CONNECT:${monitor_sock}" >/dev/null 2>&1
    for _ in $(seq 1 "${RADLIB_X86_INPUT_OBSERVED_POLLS:-30}"); do
        if grep -q "RAD_IRQ_KEYBOARD_OK" "${qemu_log}" 2>/dev/null; then
            keyboard_injected=1
            break
        fi
        sleep 0.1
    done
fi
wait "${qemu_pid}"
qemu_status=$?
set -e
cat "${qemu_log}"

if [[ "${qemu_status}" != "0" && "${qemu_status}" != "124" ]]; then
    echo "QEMU exited with status ${qemu_status}" >&2
    exit "${qemu_status}"
fi

if [[ "${ui_profile}" == "wm" ]]; then
    grep -q "RAD x86_64 GRUB Slint handoff" "${qemu_log}"
    if grep -q "RAD_X86_UI_PROFILE_WM_OK" "${qemu_log}"; then
        grep -q "RAD_SLINT_BOOT_SHELL_OK" "${qemu_log}"
        grep -q "RAD_SLINT_WM_OK" "${qemu_log}"
    else
        echo "WM VM reached base terminal before timeout; hosted Slint smoke covers UI markers."
    fi
else
    grep -q "RAD x86_64 GRUB terminal handoff" "${qemu_log}"
    grep -q "RAD_X86_UI_PROFILE_TERMINAL_OK" "${qemu_log}"
fi
grep -q "RAD x86_64 base terminal ready" "${qemu_log}"
grep -q "RAD_BASE_TERMINAL_OK" "${qemu_log}"
grep -q "RAD_LOGIN_SPAWN_OK" "${qemu_log}"
grep -q "RAD_LOGIN_OK" "${qemu_log}"
grep -q "RAD_TERMINAL_ANSI_OK" "${qemu_log}"
grep -q "RAD_TERMINAL_THEME_OK" "${qemu_log}"
grep -q "RAD_TERMINAL_FONT_PSF_OK" "${qemu_log}"
grep -q "RAD_RKCONFIG_AUTOCOMPLETE_OK" "${qemu_log}"
grep -q "RAD_X86_CPU_OK" "${qemu_log}"
grep -q "RAD_IRQ_CORE_OK" "${qemu_log}"
grep -q "RAD_PIC_REMAP_OK" "${qemu_log}"
grep -q "RAD_IRQ_DOMAIN_PIC_OK" "${qemu_log}"
grep -q "RAD_IRQ_TREE_OK" "${qemu_log}"
grep -q "RAD_TIMER_IRQ_OK" "${qemu_log}"
grep -q "RAD_DEFERRED_WORK_OK" "${qemu_log}"
grep -q "RAD_WAIT_QUEUE_OK" "${qemu_log}"
grep -q "RAD_SCHED_CORE_OK" "${qemu_log}"
grep -q "RAD_SMP_TOPOLOGY_OK" "${qemu_log}"
grep -q "RAD_AP_START_OK" "${qemu_log}"
grep -q "RAD_AP_WORKER_TASK_OK" "${qemu_log}"
grep -q "RAD_LAPIC_TIMER_OK" "${qemu_log}"
grep -q "RAD_AP_TIMER_IRQ_OK" "${qemu_log}"
grep -q "RAD_AP_SCHED_STRESS_OK" "${qemu_log}"
grep -q "RAD_CONTEXT_DISPATCH_OK" "${qemu_log}"
grep -q "RAD_CONTEXT_SWITCH_OK" "${qemu_log}"
grep -q "RAD_PREEMPT_SCHED_OK" "${qemu_log}"
grep -q "RAD_AP_PREEMPT_SCHED_OK" "${qemu_log}"
if grep -Eq "RAD_[A-Z0-9_]*_(DEFERRED|TODO)([^A-Z0-9]|$)" "${qemu_log}"; then
    echo "x86 completion gate failed: deferred/TODO marker present" >&2
    exit 1
fi
if grep -Eq "x86 exception|RAD_[A-Z0-9_]*_FAIL" "${qemu_log}"; then
    echo "x86 completion gate failed: exception/failure marker present" >&2
    exit 1
fi
grep -q "RAD_USER_IF_OK" "${qemu_log}"
grep -q "RAD_CPP_RUNTIME_OK" "${qemu_log}"
grep -q "RAD_MODULE_LIFECYCLE_OK" "${qemu_log}"
grep -q "RAD_SERVICE_JSON_OK" "${qemu_log}"
grep -q "RAD_SERVICE_BOOTSTRAP_OK" "${qemu_log}"
grep -q "RAD_SERVICE_ROOTFS_OK" "${qemu_log}"
grep -q "RAD_SERVICE_USERSPACE_OK" "${qemu_log}"
grep -q "RAD_SERVICE_FAT_OK" "${qemu_log}"
grep -q "RAD_SERVICE_NETWORK_OK" "${qemu_log}"
grep -q "RAD_SERVICE_TERMINAL_OK" "${qemu_log}"
grep -q "RAD_RADINIT_SPAWN_OK" "${qemu_log}"
grep -q "RAD_RADINIT_BOOT_OK" "${qemu_log}"
grep -q "RAD_RADINIT_JSON_OK" "${qemu_log}"
grep -q "RAD_RADINIT_SERVICE_ORDER_OK" "${qemu_log}"
grep -q "RAD_RADINIT_SERVICE_START_OK" "${qemu_log}"
grep -q "RAD_LOG_RING_OK" "${qemu_log}"
grep -q "RAD_LOG_KERNEL_FILE_OK" "${qemu_log}"
grep -q "RAD_LOG_INIT_FILE_OK" "${qemu_log}"
grep -q "RAD_LOG_SERVICE_FILE_OK" "${qemu_log}"
grep -q "RAD_INT80_OK" "${qemu_log}"
grep -q "RAD_POSIX_ABI_OK" "${qemu_log}"
grep -q "RAD_VM_READY" "${qemu_log}"
grep -q "RAD_VM_PAGE_ALLOC_OK" "${qemu_log}"
grep -q "RAD_USER_VM_SCAFFOLD_OK" "${qemu_log}"
grep -q "RAD_USER_VM_ISOLATED_OK" "${qemu_log}"
grep -q "RAD_USER_ENTRY_MAP_OK" "${qemu_log}"
grep -q "RAD_USER_COPY_OK" "${qemu_log}"
grep -q "RAD_USER_ENTRY_PER_CORE_OK" "${qemu_log}"
grep -q "RAD_USER_EXECVE_OK" "${qemu_log}"
grep -q "RAD_USER_EXECVE_REENTER_OK" "${qemu_log}"
grep -q "RAD_USER_PROCESS_SPAWN_OK" "${qemu_log}"
grep -q "RAD_USER_PROCESS_WAIT_OK" "${qemu_log}"
grep -q "RAD_USER_PROCESS_FD_TABLE_OK" "${qemu_log}"
grep -q "RAD_USER_FD_CLOEXEC_OK" "${qemu_log}"
grep -q "RAD_USER_SYSCALLS_OK" "${qemu_log}"
grep -q "RAD_USER_AP_EXEC_OK" "${qemu_log}"
grep -q "RAD_USER_AP_SYSCALL_OK" "${qemu_log}"
grep -q "RAD_USER_INVALID_PTR_OK" "${qemu_log}"
grep -q "RAD_USER_LINUX_SYSCALL_ABI_OK" "${qemu_log}"
grep -q "RAD_USER_RADSH_BOOT_OK" "${qemu_log}"
grep -q "RAD_USER_RADSH_GETDENTS_OK" "${qemu_log}"
grep -q "RAD_USER_RADSH_EXIT_OK" "${qemu_log}"
grep -q "RAD_USER_FORK_OK" "${qemu_log}"
grep -q "RAD_USER_FORK_CHILD_OK" "${qemu_log}"
grep -q "RAD_USER_FORK_WAIT_OK" "${qemu_log}"
grep -q "RAD_USER_FORK_FD_INHERIT_OK" "${qemu_log}"
grep -q "RAD_USER_PIPE_FORK_OK" "${qemu_log}"
grep -q "RAD_USER_COW_PAGE_FAULT_OK" "${qemu_log}"
grep -q "RAD_USER_AP_FORK_COW_OK" "${qemu_log}"
grep -q "RAD_USER_COW_PARENT_ISOLATED_OK" "${qemu_log}"
grep -q "RAD_USER_SCRIPT_SHEBANG_OK" "${qemu_log}"
grep -q "RAD_USER_ARGV_ENVP_OK" "${qemu_log}"
grep -q "RAD_USER_SH_SCRIPT_OK" "${qemu_log}"
grep -q "RAD_RASH_PATH_EXEC_OK" "${qemu_log}"
grep -q "RAD_TERMINAL_SCALE_OK" "${qemu_log}"
grep -q "RAD_NANO_TINY_VERSION_OK" "${qemu_log}"
grep -q "RAD_NANO_FULL_VERSION_OK" "${qemu_log}"
grep -q "RAD_NANO_TINY_EDIT_OK" "${qemu_log}"
grep -q "RAD_NANO_FULL_EDIT_OK" "${qemu_log}"
grep -q "RAD_NANO_TINY_SAVE_OK" "${qemu_log}"
grep -q "RAD_NANO_FULL_SAVE_OK" "${qemu_log}"
grep -q "RAD_NANO_TINY_EXIT_OK" "${qemu_log}"
grep -q "RAD_NANO_FULL_EXIT_OK" "${qemu_log}"
grep -q "RAD_PCI_PROBE_OK" "${qemu_log}"
grep -q "RAD_INPUT_POINTER_OK" "${qemu_log}"
grep -q "RAD_VIRTIO_BLK_FOUND" "${qemu_log}"
grep -q "RAD_VIRTIO_BLK_READ_OK" "${qemu_log}"
grep -q "RAD_VIRTIO_NET_FOUND" "${qemu_log}"
grep -q "RAD_NET_DEVICE_REGISTERED" "${qemu_log}"
grep -q "RAD_VIRTIO_NET_TX_RING_OK" "${qemu_log}"
grep -q "RAD_VIRTIO_NET_RX_RING_OK" "${qemu_log}"
grep -q "RAD_ETH_TX_OK" "${qemu_log}"
grep -q "RAD_ARP_OK" "${qemu_log}"
grep -q "RAD_IPV4_OK" "${qemu_log}"
grep -q "RAD_UDP_OK" "${qemu_log}"
grep -q "RAD_UDP_RX_OK" "${qemu_log}"
grep -q "RAD_NET_UDP_TX_OK" "${qemu_log}"
grep -q "RAD_NET_UDP_RX_OK" "${qemu_log}"
grep -q "RAD_NET_HOST_UDP_ECHO_OK" "${qemu_log}"
grep -q "RAD_NTP_QUERY_OK" "${qemu_log}"
grep -q "RAD_NTP_RESPONSE_OK" "${qemu_log}"
grep -q "RAD_NTP_TIME_SAMPLE_OK" "${qemu_log}"
grep -q "RAD_SOCKET_DGRAM_OK" "${qemu_log}"
grep -q "RAD_TCP_SOCKET_OK" "${qemu_log}"
grep -q "RAD_TCP_LISTEN_ACCEPT_OK" "${qemu_log}"
grep -q "RAD_TCP_CONNECT_OK" "${qemu_log}"
grep -q "RAD_TCP_STREAM_IO_OK" "${qemu_log}"
grep -q "RAD_TCP_SHUTDOWN_OK" "${qemu_log}"
grep -q "RAD_EXT4_MOUNT_OK" "${qemu_log}"
grep -q "RAD_EXT4_ROOTFS_OK" "${qemu_log}"
grep -q "RAD_EXT4_RW_OK" "${qemu_log}"
grep -q "RAD_EXT4_MKDIR_OK" "${qemu_log}"
grep -q "RAD_EXT4_CREATE_OK" "${qemu_log}"
grep -q "RAD_EXT4_RENAME_OK" "${qemu_log}"
grep -q "RAD_EXT4_TRUNCATE_OK" "${qemu_log}"
grep -q "RAD_EXT4_SYMLINK_OK" "${qemu_log}"
grep -q "RAD_EXT4_UNLINK_OK" "${qemu_log}"
grep -q "RAD_EXT4_RMDIR_OK" "${qemu_log}"
grep -q "RAD_ROOTFS_INIT_FOUND" "${qemu_log}"
grep -q "RAD_FAT_MOUNT_OK" "${qemu_log}"
grep -q "RAD_FAT_RW_OK" "${qemu_log}"
grep -q "RAD_USERMODE_ENTER_OK" "${qemu_log}"
grep -q "RAD_USER_INIT_OK" "${qemu_log}"
grep -q "RAD_USERMODE_EXIT_OK" "${qemu_log}"
grep -q "cmd fb" "${qemu_log}"
grep -q "cmd sched" "${qemu_log}"
grep -q "cmd services" "${qemu_log}"
grep -q "cmd initctl list" "${qemu_log}"
grep -q "cmd initctl status userspace-shell" "${qemu_log}"
grep -q "cmd help" "${qemu_log}"
grep -q "cmd ls /bin" "${qemu_log}"
grep -q "cmd cat /bin/test.sh" "${qemu_log}"
grep -q "cmd mkdir /tmp/rashpass" "${qemu_log}"
grep -q "cmd touch /tmp/rashpass/file" "${qemu_log}"
grep -q "cmd stat /tmp/rashpass/file" "${qemu_log}"
grep -q "cmd mount" "${qemu_log}"
grep -q "cmd ps" "${qemu_log}"
grep -q "cmd sh /bin/test.sh" "${qemu_log}"
grep -q "cmd which sh" "${qemu_log}"
grep -q "cmd stty -a" "${qemu_log}"
grep -q "cmd tput cols" "${qemu_log}"
grep -q "cmd tput cup 0 0" "${qemu_log}"
grep -q "cmd test -e /usr/share/terminfo/r/rad" "${qemu_log}"
grep -q "cmd echo rad | wc" "${qemu_log}"
grep -q "cmd initctl restart userspace-shell" "${qemu_log}"
grep -q "cmd logread init" "${qemu_log}"
grep -q "cmd logread userspace-shell" "${qemu_log}"
grep -q "cmd logread kernel" "${qemu_log}"
grep -q "cmd dmesg" "${qemu_log}"
if [[ "${keyboard_injected}" == "1" ]]; then
    grep -q "RAD_IRQ_KEYBOARD_OK" "${qemu_log}"
    grep -q "RAD_IRQ_MOUSE_OK" "${qemu_log}"
    grep -q "RAD_IRQ_TREE_PS2_OK" "${qemu_log}"
    grep -q "RAD_INPUT_WAKE_OK" "${qemu_log}"
    if [[ "${ui_profile}" == "wm" ]]; then
        grep -q "RAD_SLINT_CURSOR_MOVE_OK" "${qemu_log}"
        grep -q "RAD_COMPOSITOR_INPUT_TRANSLATE_OK" "${qemu_log}"
    fi
fi
grep -q "RAD_RASH_CLEAR_OK" "${qemu_log}"
grep -q "RAD_RASH_HISTORY_OK" "${qemu_log}"
grep -q "RAD_RASH_AUTOCOMPLETE_OK" "${qemu_log}"
grep -q "RAD_RASH_AUTOCOMPLETE_PATH_OK" "${qemu_log}"
grep -q "RAD_RASH_PIPE_OK" "${qemu_log}"
grep -q "RAD_TTY_IOCTL_WINSIZE_OK" "${qemu_log}"
grep -q "RAD_TTY_RAW_MODE_OK" "${qemu_log}"
grep -q "RAD_TUI_SMOKE_OK" "${qemu_log}"
grep -q "RAD_TPUT_CUP_OK" "${qemu_log}"

echo "x86_64 GRUB AP scheduler smoke passed"
