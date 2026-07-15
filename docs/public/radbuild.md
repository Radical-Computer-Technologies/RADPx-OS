# RadBuild Integration {#radbuild_integration}

RADix-OS uses RadBuild 0.2.1 as the canonical graph entrypoint for repeatable
x86 VM image builds. Crimson defines standalone RadBuild project JSON files for
the terminal and RADCompositor/Slint profiles, plus a root `settings.json` that
can build both profiles together.

## Build Command

Run from the RADix-OS repository root:

```bash
radbuild build os --settings settings.json --json-events
radbuild build os --settings settings.terminal.json --json-events
radbuild build os --settings settings.wm.json --json-events
```

The provider executes `tools/embedded/x86_64_grub_slint_smoke.sh` with the
configured `ui_profile` for each QEMU SMP count. The terminal profile is the
default VM boot gate. The WM profile is the RADCompositor/Slint UI package
gate: it builds the Slint/RADCompositor image and runs the hosted UI smoke,
while full Slint startup inside the VM remains a separate stabilization item.

## Artifact Output

Successful builds stage development artifacts under:

```text
artifacts/radix/x86_64-grub-terminal/
artifacts/radix/x86_64-grub-wm/
```

The staged set includes the GRUB ISO, kernel ELF, ext4 root filesystem, FAT32
image, `run-radix-vm.sh`, one serial log per SMP smoke, and `SHA256SUMS`.

The terminal profile excludes Slint/RADCompositor chunks and boots directly to
the framebuffer terminal. The WM profile includes the Slint-backed
RADCompositor shell.

## rkconfig and Root Filesystem Layout

Each `os.radix` system can provide a `config.rkconfig` object. The current
Crimson fields are:

- `hostname`: written to `/etc/hostname` and used by the `rash` prompt.
- `root_password`: hashed into `/etc/passwords` for the initial root login.
- `rootfs_size_mb`: ext4 image size for `/`.
- `terminal_scale`: framebuffer terminal scale, or `auto`.
- `terminal_font`: framebuffer terminal font selection.
- `terminal_theme`: framebuffer terminal color/theme selection.
- `terminal_autocomplete`: enables `rash` tab completion.
- `terminal_posix_compat`: enables the POSIX terminal compatibility layer.
- `terminal_ncurses`: stages the RADix terminfo/sysroot readiness pieces for
  ncurses-style ports.
- `terminal_nano`: disabled by default; reserved for the experimental upstream
  nano port while TTY/libc compatibility matures.
- `terminal_vim`: disabled by default; enables the experimental upstream
  vim-tiny port when RadBuild-managed Vim source is available.
- `kernel_max_tasks` and `kernel_max_processes`: scheduler/process table
  capacity, defaulting to 128 for the x86 terminal profile.
- `kernel_task_stack_bytes` and `kernel_task_stack_policy`: default kernel task
  stack size and `dynamic` or `static` allocation policy. x86 defaults to
  dynamic 512 KiB task stacks.

The generated ext4 image stages a Unix-like base layout:

```text
/bin /dev /etc /home/root /lib /lib/radix/modules /mnt/fat /sbin /tmp /usr/bin /usr/lib /var/log
```

Kernel modules use the `.rko` extension under `/lib/radix/modules`. Dynamic
shared objects use `.rso` under `/lib` or `/usr/lib` once the runtime dynamic
linker lands. These names are RADix-specific and intentionally do not collide
with Linux `.ko` kernel modules or `.so` shared libraries.

RadBuild writes the machine-readable manifest at:

```text
.radmeta/artifacts/radbuild-artifacts.json
```

`artifacts/` and `.radmeta/` are local build outputs and are intentionally not
tracked in the RADix-OS source repository.

## Publication and Packagegroups

RADix-OS source documentation, generated API docs, VM image bundles, and
`.radpm` packages are published through RadicalPackages experimental releases.
RadBuild can consume packagegroups from that repository during image creation:

```json
{
  "radpm": {
    "repository": "https://github.com/Radical-Computer-Technologies/RadicalPackages/releases/download/radix-os-0.2.0-beta.1",
    "suite": "experimental",
    "packagegroups": ["radix-terminal-base", "radix-networking"],
    "packages": [],
    "verify_signatures": true
  }
}
```

The packagegroup model keeps rootfs contents explicit while allowing RadBuild
to explain missing dependencies and available providers.
