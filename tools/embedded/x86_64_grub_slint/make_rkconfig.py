#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path


def password_hash(salt: str, password: str) -> str:
    return hashlib.sha256(f"{salt}:{password}".encode("utf-8")).hexdigest()


def bool_arg(value: str) -> bool:
    return str(value).strip().lower() in ("1", "true", "yes", "on", "y")


def parse_ipv4(value: str, fallback: str) -> tuple[int, int, int, int]:
    text = str(value or fallback).strip()
    parts = text.split(".")
    if len(parts) != 4:
        text = fallback
        parts = text.split(".")
    octets = []
    for part in parts:
        try:
            octet = int(part, 10)
        except ValueError:
            return parse_ipv4(fallback, fallback)
        if octet < 0 or octet > 255:
            return parse_ipv4(fallback, fallback)
        octets.append(octet)
    return tuple(octets)  # type: ignore[return-value]


def looks_ipv4(value: str) -> bool:
    text = str(value or "").strip()
    parts = text.split(".")
    if len(parts) != 4:
        return False
    try:
        return all(0 <= int(part, 10) <= 255 for part in parts)
    except ValueError:
        return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--hostname", default="radix")
    parser.add_argument("--root-password", default="radix")
    parser.add_argument("--rootfs-size-mb", default="256")
    parser.add_argument("--terminal-scale", default="auto")
    parser.add_argument("--terminal-font", default="radix-default")
    parser.add_argument("--terminal-theme", default="radix-dark")
    parser.add_argument("--terminal-autocomplete", default="true")
    parser.add_argument("--terminal-posix-compat", default="true")
    parser.add_argument("--terminal-ncurses", default="true")
    parser.add_argument("--terminal-nano", default="false")
    parser.add_argument("--terminal-nano-variant", default="none")
    parser.add_argument("--terminal-vim", default="false")
    parser.add_argument("--terminal-vim-variant", default="none")
    parser.add_argument("--network-enabled", default="true")
    parser.add_argument("--net-ipv4", default="10.0.2.15")
    parser.add_argument("--net-netmask", default="255.255.255.0")
    parser.add_argument("--net-gateway", default="10.0.2.2")
    parser.add_argument("--net-ntp-server", default="time.google.com")
    parser.add_argument("--net-ntp-port", default="123")
    parser.add_argument("--net-dns-server", default="10.0.2.3")
    parser.add_argument("--timezone", default="America/Los_Angeles")
    parser.add_argument("--dns-search", default="local")
    parser.add_argument("--kernel-max-tasks", default="128")
    parser.add_argument("--kernel-max-processes", default="128")
    parser.add_argument("--kernel-task-stack-bytes", default="524288")
    parser.add_argument("--kernel-task-stack-policy", default="dynamic")
    args = parser.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    salt = "radix-root-v1"
    digest = password_hash(salt, args.root_password)
    version = "0.1.3"
    autocomplete = bool_arg(args.terminal_autocomplete)
    posix_compat = bool_arg(args.terminal_posix_compat)
    ncurses = bool_arg(args.terminal_ncurses)
    nano = bool_arg(args.terminal_nano)
    nano_variant = str(args.terminal_nano_variant).strip().lower()
    if nano_variant not in ("none", "tiny", "full", "both"):
        nano_variant = "none"
    if nano_variant != "none":
        nano = True
    vim = bool_arg(args.terminal_vim)
    vim_variant = str(args.terminal_vim_variant).strip().lower()
    if vim_variant not in ("none", "tiny"):
        vim_variant = "none"
    if vim and vim_variant == "none":
        vim_variant = "tiny"
    if vim_variant != "none":
        vim = True
    stack_policy = str(args.kernel_task_stack_policy).strip().lower()
    if stack_policy not in ("dynamic", "static"):
        stack_policy = "dynamic"
    network_enabled = bool_arg(args.network_enabled)
    net_ipv4 = parse_ipv4(args.net_ipv4, "10.0.2.15")
    net_netmask = parse_ipv4(args.net_netmask, "255.255.255.0")
    net_gateway = parse_ipv4(args.net_gateway, "10.0.2.2")
    net_ntp_host = str(args.net_ntp_server or "time.google.com").strip() or "time.google.com"
    net_ntp_server = parse_ipv4(net_ntp_host, "216.239.35.0")
    net_dns_server = parse_ipv4(args.net_dns_server, "10.0.2.3")
    try:
        net_ntp_port = int(str(args.net_ntp_port), 10)
    except ValueError:
        net_ntp_port = 123
    if net_ntp_port <= 0 or net_ntp_port > 65535:
        net_ntp_port = 123

    (out / "rkconfig").write_text(
        "\n".join(
            [
                "CONFIG_RADIX_OS=y",
                f'CONFIG_RADIX_HOSTNAME="{args.hostname}"',
                f"CONFIG_RADIX_ROOTFS_SIZE_MB={args.rootfs_size_mb}",
                f'CONFIG_RADIX_TERMINAL_SCALE="{args.terminal_scale}"',
                f'CONFIG_RADIX_TERMINAL_FONT="{args.terminal_font}"',
                f'CONFIG_RADIX_TERMINAL_THEME="{args.terminal_theme}"',
                f"CONFIG_RADIX_TERMINAL_AUTOCOMPLETE={'y' if autocomplete else 'n'}",
                f"CONFIG_RADIX_TERMINAL_POSIX_COMPAT={'y' if posix_compat else 'n'}",
                f"CONFIG_RADIX_TERMINAL_NCURSES={'y' if ncurses else 'n'}",
                f"CONFIG_RADIX_TERMINAL_NANO={'y' if nano else 'n'}",
                f'CONFIG_RADIX_TERMINAL_NANO_VARIANT="{nano_variant}"',
                f"CONFIG_RADIX_TERMINAL_VIM={'y' if vim else 'n'}",
                f'CONFIG_RADIX_TERMINAL_VIM_VARIANT="{vim_variant}"',
                f"CONFIG_RADIX_NETWORK={'y' if network_enabled else 'n'}",
                f'CONFIG_RADIX_NET_IPV4="{args.net_ipv4}"',
                f'CONFIG_RADIX_NET_NETMASK="{args.net_netmask}"',
                f'CONFIG_RADIX_NET_GATEWAY="{args.net_gateway}"',
                f'CONFIG_RADIX_NET_NTP_SERVER="{net_ntp_host}"',
                f"CONFIG_RADIX_NET_NTP_PORT={net_ntp_port}",
                f'CONFIG_RADIX_NET_DNS_SERVER="{args.net_dns_server}"',
                f"CONFIG_RADIX_KERNEL_MAX_TASKS={args.kernel_max_tasks}",
                f"CONFIG_RADIX_KERNEL_MAX_PROCESSES={args.kernel_max_processes}",
                f"CONFIG_RADIX_KERNEL_TASK_STACK_BYTES={args.kernel_task_stack_bytes}",
                f'CONFIG_RADIX_KERNEL_TASK_STACK_POLICY="{stack_policy}"',
                'CONFIG_RADIX_TERMINAL_PALETTE_BACKGROUND="#090716"',
                'CONFIG_RADIX_TERMINAL_PALETTE_FOREGROUND="#d8f7ee"',
                'CONFIG_RADIX_TERMINAL_PALETTE_PROMPT="#4ade80"',
                "CONFIG_RADIX_AUTH_PASSWORDS=y",
                "CONFIG_RADIX_MODULE_EXT_RKO=y",
                "CONFIG_RADIX_SHARED_OBJECT_EXT_RSO=y",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (out / "rkconfig.h").write_text(
        "\n".join(
            [
                "#ifndef RADIX_GENERATED_RKCONFIG_H",
                "#define RADIX_GENERATED_RKCONFIG_H",
                f'#define RADIX_RKCONFIG_HOSTNAME "{args.hostname}"',
                f"#define RADIX_RKCONFIG_ROOTFS_SIZE_MB {args.rootfs_size_mb}",
                f'#define RADIX_RKCONFIG_TERMINAL_SCALE "{args.terminal_scale}"',
                f'#define RADIX_RKCONFIG_TERMINAL_FONT "{args.terminal_font}"',
                f'#define RADIX_RKCONFIG_TERMINAL_THEME "{args.terminal_theme}"',
                f"#define RADIX_RKCONFIG_TERMINAL_AUTOCOMPLETE {1 if autocomplete else 0}",
                f"#define RADIX_RKCONFIG_TERMINAL_POSIX_COMPAT {1 if posix_compat else 0}",
                f"#define RADIX_RKCONFIG_TERMINAL_NCURSES {1 if ncurses else 0}",
                f"#define RADIX_RKCONFIG_TERMINAL_NANO {1 if nano else 0}",
                f'#define RADIX_RKCONFIG_TERMINAL_NANO_VARIANT "{nano_variant}"',
                f"#define RADIX_RKCONFIG_TERMINAL_VIM {1 if vim else 0}",
                f'#define RADIX_RKCONFIG_TERMINAL_VIM_VARIANT "{vim_variant}"',
                f"#define RADIX_RKCONFIG_NETWORK {1 if network_enabled else 0}",
                f"#define RADIX_RKCONFIG_NET_IPV4_A {net_ipv4[0]}",
                f"#define RADIX_RKCONFIG_NET_IPV4_B {net_ipv4[1]}",
                f"#define RADIX_RKCONFIG_NET_IPV4_C {net_ipv4[2]}",
                f"#define RADIX_RKCONFIG_NET_IPV4_D {net_ipv4[3]}",
                f"#define RADIX_RKCONFIG_NET_NETMASK_A {net_netmask[0]}",
                f"#define RADIX_RKCONFIG_NET_NETMASK_B {net_netmask[1]}",
                f"#define RADIX_RKCONFIG_NET_NETMASK_C {net_netmask[2]}",
                f"#define RADIX_RKCONFIG_NET_NETMASK_D {net_netmask[3]}",
                f"#define RADIX_RKCONFIG_NET_GATEWAY_A {net_gateway[0]}",
                f"#define RADIX_RKCONFIG_NET_GATEWAY_B {net_gateway[1]}",
                f"#define RADIX_RKCONFIG_NET_GATEWAY_C {net_gateway[2]}",
                f"#define RADIX_RKCONFIG_NET_GATEWAY_D {net_gateway[3]}",
                f'#define RADIX_RKCONFIG_NET_NTP_HOST "{net_ntp_host}"',
                f"#define RADIX_RKCONFIG_NET_NTP_A {net_ntp_server[0]}",
                f"#define RADIX_RKCONFIG_NET_NTP_B {net_ntp_server[1]}",
                f"#define RADIX_RKCONFIG_NET_NTP_C {net_ntp_server[2]}",
                f"#define RADIX_RKCONFIG_NET_NTP_D {net_ntp_server[3]}",
                f"#define RADIX_RKCONFIG_NET_NTP_PORT {net_ntp_port}",
                f"#define RADIX_RKCONFIG_NET_DNS_A {net_dns_server[0]}",
                f"#define RADIX_RKCONFIG_NET_DNS_B {net_dns_server[1]}",
                f"#define RADIX_RKCONFIG_NET_DNS_C {net_dns_server[2]}",
                f"#define RADIX_RKCONFIG_NET_DNS_D {net_dns_server[3]}",
                f"#define RADIX_RKCONFIG_KERNEL_MAX_TASKS {args.kernel_max_tasks}",
                f"#define RADIX_RKCONFIG_KERNEL_MAX_PROCESSES {args.kernel_max_processes}",
                f"#define RADIX_RKCONFIG_KERNEL_TASK_STACK_BYTES {args.kernel_task_stack_bytes}",
                f'#define RADIX_RKCONFIG_KERNEL_TASK_STACK_POLICY "{stack_policy}"',
                "#define RADIX_RKCONFIG_AUTH_PASSWORDS 1",
                "#endif",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (out / "issue").write_text(f"RADix OS {version} x86_64\nKernel API {version}\n\n", encoding="utf-8")
    (out / "hostname").write_text(f"{args.hostname}\n", encoding="utf-8")
    (out / "terminal.theme").write_text(
        "\n".join(
            [
                f"name={args.terminal_theme}",
                f"font={args.terminal_font}",
                f"scale={args.terminal_scale}",
                f"autocomplete={'true' if autocomplete else 'false'}",
                f"posix_compat={'true' if posix_compat else 'false'}",
                f"ncurses={'true' if ncurses else 'false'}",
                f"nano={'true' if nano else 'false'}",
                f"nano_variant={nano_variant}",
                f"vim={'true' if vim else 'false'}",
                f"vim_variant={vim_variant}",
                f"network={'true' if network_enabled else 'false'}",
                f"net_ipv4={args.net_ipv4}",
                f"net_netmask={args.net_netmask}",
                f"net_gateway={args.net_gateway}",
                f"net_ntp_server={net_ntp_host}",
                f"net_ntp_port={net_ntp_port}",
                f"net_dns_server={args.net_dns_server}",
                "background=#090716",
                "foreground=#d8f7ee",
                "cursor=#f8fafc",
                "prompt=#4ade80",
                "directory=#60a5fa",
                "executable=#4ade80",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (out / "radpm-index.json").write_text(
        '{\n  "schema": "radix-radpm-index",\n  "schema_version": 1,\n  "packages": []\n}\n',
        encoding="utf-8",
    )
    (out / "hosts").write_text(
        "\n".join(
            [
                "127.0.0.1 localhost",
                f"{args.net_ipv4} {args.hostname}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (out / "resolv.conf").write_text(
        "\n".join(
            [
                f"nameserver {args.net_dns_server}",
                "options timeout:1 attempts:2",
                "",
            ]
        ),
        encoding="utf-8",
    )
    network_dir = out / "network"
    time_dir = out / "time"
    zoneinfo_dir = out / "zoneinfo"
    network_dir.mkdir(parents=True, exist_ok=True)
    time_dir.mkdir(parents=True, exist_ok=True)
    (network_dir / "netinterfaces.json").write_text(
        "\n".join(
            [
                "{",
                '  "schema": "radix-netinterfaces",',
                '  "schema_version": 1,',
                '  "interfaces": [',
                "    {",
                '      "name": "net0",',
                '      "device": "/dev/net0",',
                f'      "enabled": {"true" if network_enabled else "false"},',
                '      "method": "static",',
                f'      "ipv4": "{args.net_ipv4}",',
                f'      "netmask": "{args.net_netmask}",',
                f'      "gateway": "{args.net_gateway}",',
                '      "mtu": 1500,',
                f'      "ntp_server": "{net_ntp_host}",',
                f'      "ntp_server_ip": "{net_ntp_server[0]}.{net_ntp_server[1]}.{net_ntp_server[2]}.{net_ntp_server[3]}",',
                f'      "ntp_port": {net_ntp_port}',
                "    }",
                "  ]",
                "}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (network_dir / "dns-resolver.json").write_text(
        "\n".join(
            [
                "{",
                '  "schema": "radix-dns-resolver",',
                '  "schema_version": 1,',
                f'  "nameserver": "{args.net_dns_server}",',
                f'  "search": "{args.dns_search}",',
                '  "options": {"timeout": 1, "attempts": 2}',
                "}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (time_dir / "time-sync.json").write_text(
        "\n".join(
            [
                "{",
                '  "schema": "radix-time-sync",',
                '  "schema_version": 1,',
                f'  "servers": ["{net_ntp_host}"],',
                f'  "server": "{net_ntp_host}",',
                f'  "port": {net_ntp_port},',
                '  "set_clock": 1,',
                '  "sync_on_boot": true,',
                '  "retry_count": 3,',
                '  "sync_interval_seconds": 3600,',
                '  "poll_interval_seconds": 3600,',
                f'  "timezone": "{args.timezone}"',
                "}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (time_dir / "timezone").write_text(f"{args.timezone}\n", encoding="utf-8")
    (time_dir / "localtime").write_text(f"TZ={args.timezone}\n", encoding="utf-8")
    zone_file = zoneinfo_dir / args.timezone
    zone_file.parent.mkdir(parents=True, exist_ok=True)
    zone_file.write_text(
        "\n".join(
            [
                "# RADix tzdata package marker.",
                "# Full binary TZif coverage is packaged by radix-tzdata releases as the timezone loader grows.",
                f"TZID={args.timezone}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    (out / "passwd").write_text("root:x:0:0:root:/home/root:/bin/rash\n", encoding="utf-8")
    (out / "passwords").write_text(
        f"root:0:0:{salt}:{digest}:/home/root:/bin/rash\n",
        encoding="utf-8",
    )
    (out / "rko-note.txt").write_text(
        ".rko files are trusted RADix kernel objects and must be loaded only from privileged module paths.\n",
        encoding="utf-8",
    )
    (out / "rso-note.txt").write_text(
        ".rso files are RADix shared objects; future POSIX compatibility may add .so aliases.\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
