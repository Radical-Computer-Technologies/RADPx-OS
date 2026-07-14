#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path


def password_hash(salt: str, password: str) -> str:
    return hashlib.sha256(f"{salt}:{password}".encode("utf-8")).hexdigest()


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
    autocomplete = str(args.terminal_autocomplete).strip().lower() in ("1", "true", "yes", "on", "y")
    posix_compat = str(args.terminal_posix_compat).strip().lower() in ("1", "true", "yes", "on", "y")
    ncurses = str(args.terminal_ncurses).strip().lower() in ("1", "true", "yes", "on", "y")
    nano = str(args.terminal_nano).strip().lower() in ("1", "true", "yes", "on", "y")
    nano_variant = str(args.terminal_nano_variant).strip().lower()
    if nano_variant not in ("none", "tiny", "full", "both"):
        nano_variant = "none"
    if nano_variant != "none":
        nano = True
    vim = str(args.terminal_vim).strip().lower() in ("1", "true", "yes", "on", "y")
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
