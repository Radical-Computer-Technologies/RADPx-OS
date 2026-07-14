#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 4 ]; then
    echo "usage: $0 <tty-stress> <sleep-stress> <signal-stress> <tui-stress> [log]" >&2
    exit 2
fi

tty_stress="$1"
sleep_stress="$2"
signal_stress="$3"
tui_stress="$4"
log_path="${5:-}"
tmp_log="$(mktemp)"
trap 'rm -f "$tmp_log"' EXIT

append_log() {
    tee -a "$tmp_log"
}

run_plain() {
    name="$1"
    shift
    echo "== ${name} =="
    "$@" | append_log
}

run_pty() {
    name="$1"
    binary="$2"
    echo "== ${name} =="
    if command -v script >/dev/null 2>&1; then
        escaped="$(printf '%q' "$binary")"
        TERM="${TERM:-xterm-256color}" script -qfec "stty rows 30 cols 100; $escaped" /dev/null | tr -d '\r' | append_log
    else
        echo "host-stress-fail:script-missing" | append_log
        return 1
    fi
}

run_plain sleep "$sleep_stress"
run_plain signal "$signal_stress"
run_pty tty "$tty_stress"
run_pty tui "$tui_stress"

required_markers=(
    RADIX_USER_NANOSLEEP_OK
    RADIX_USER_POLL_TIMEOUT_OK
    RADIX_SCHED_WAKE_STRESS_OK
    RADIX_SIGNAL_TABLE_OK
    RADIX_SIGNAL_IGNORE_OK
    RADIX_SIGNAL_STRESS_OK
    RADIX_TTY_RAW_STRESS_OK
    RADIX_TTY_CBREAK_STRESS_OK
    RADIX_PTY_POLL_STRESS_OK
    RADIX_NCURSES_INIT_OK
    RADIX_NCURSES_CURSOR_OK
    RADIX_NCURSES_INPUT_OK
    RADIX_NCURSES_EXIT_OK
    RADIX_NCURSES_PARITY_OK
)

for marker in "${required_markers[@]}"; do
    if ! grep -q "$marker" "$tmp_log"; then
        echo "host-stress-missing:${marker}" >&2
        [ -n "$log_path" ] && cp "$tmp_log" "$log_path"
        exit 1
    fi
done

echo "RADIX_HOST_TERMINAL_STRESS_OK" | append_log
[ -n "$log_path" ] && cp "$tmp_log" "$log_path"
