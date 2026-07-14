#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

export RADIX_X86_UI_PROFILE=terminal
export RADIX_X86_TERMINAL_AUTOTEST_NANO=false
export RADIX_X86_TERMINAL_AUTOLOGIN=false
export RADLIB_X86_64_GRUB_SLINT_BUILD_DIR="${RADLIB_X86_64_GRUB_SLINT_BUILD_DIR:-${repo_root}/build/embedded/x86_64_grub_terminal_interactive}"

exec "${script_dir}/run_x86_64_grub_slint_vm.sh" "$@"
