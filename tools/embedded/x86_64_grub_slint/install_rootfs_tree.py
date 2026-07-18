#!/usr/bin/env python3
"""Install a host directory tree into a RADPx ext filesystem image with debugfs."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def debugfs(image: Path, command: str, quiet: bool = False) -> None:
    stderr = subprocess.DEVNULL if quiet else None
    subprocess.run(["debugfs", "-w", "-R", command, str(image)], check=True, stdout=subprocess.DEVNULL, stderr=stderr)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: install_rootfs_tree.py <rootfs-image> <tree>", file=sys.stderr)
        return 2
    image = Path(sys.argv[1])
    if not sys.argv[2]:
        return 0
    tree = Path(sys.argv[2])
    if not tree.exists():
        return 0
    if not tree.is_dir():
        print(f"RADPx package rootfs payload is not a directory: {tree}", file=sys.stderr)
        return 2

    for directory in sorted((item for item in tree.rglob("*") if item.is_dir()), key=lambda item: len(item.parts)):
        rel = "/" + directory.relative_to(tree).as_posix()
        try:
            debugfs(image, f"mkdir {rel}", quiet=True)
        except subprocess.CalledProcessError:
            pass

    for file_path in sorted(item for item in tree.rglob("*") if item.is_file()):
        rel = "/" + file_path.relative_to(tree).as_posix()
        debugfs(image, f"write {file_path} {rel}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
