#!/usr/bin/env python3
"""Safely format a removable Windows SD card for WatcheRobot."""

from __future__ import annotations

import argparse
import platform
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable

try:
    from .install_sd_card_resources import DriveInfo, inspect_explicit_drive
except ImportError:
    from install_sd_card_resources import DriveInfo, inspect_explicit_drive


FILESYSTEM = "FAT32"
ALLOCATION_UNIT_BYTES = 512
VOLUME_LABEL = "WATCHE"
MAX_WINDOWS_FAT32_BYTES = 32 * 1024 * 1024 * 1024


class FormatError(RuntimeError):
    pass


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="tools/flash sd-format",
        description="Format a removable Windows SD card as FAT32 with a 512-byte allocation unit.",
    )
    parser.add_argument("--drive", type=Path, required=True, help=r"SD-card root, for example F:\.")
    return parser.parse_args(argv)


def _drive_token(root: Path) -> str:
    match = re.fullmatch(r"([A-Za-z]):\\?", str(root).strip().replace("/", "\\"))
    if not match:
        raise FormatError(r"The format target must be a Windows drive root such as F:\.")
    return match.group(1).upper() + ":"


def validate_format_target(drive: DriveInfo) -> str:
    token = _drive_token(drive.root)
    if drive.kind != "removable":
        raise FormatError(f"Refusing to format {token}: the target is not reported as removable media.")
    if drive.total_bytes <= 0 or drive.total_bytes > MAX_WINDOWS_FAT32_BYTES:
        raise FormatError(
            f"Refusing to format {token}: Windows FAT32 formatting supports cards up to 32 GiB in this tool."
        )
    return token


def format_card(
    target: Path,
    prompt: Callable[[str], str] = input,
    runner: Callable[..., subprocess.CompletedProcess] = subprocess.run,
) -> DriveInfo:
    if platform.system() != "Windows":
        raise FormatError("The automatic SD formatter is currently supported on Windows only.")

    drive = inspect_explicit_drive(target)
    token = validate_format_target(drive)
    print(f"Target : {drive.root}  {drive.label or '(no label)'}  {drive.total_bytes / (1024 * 1024):.1f} MB")
    print("Format : FAT32, 512-byte allocation unit")
    print("WARNING: formatting permanently deletes all files on this card, including resources and creator works.")
    answer = prompt(f"Type {token} to confirm formatting, or press Enter to cancel: ").strip().upper()
    if answer != token:
        raise FormatError("Formatting cancelled; the confirmation did not match the target drive.")

    refreshed = inspect_explicit_drive(target)
    validate_format_target(refreshed)
    if (refreshed.root, refreshed.total_bytes, refreshed.label) != (drive.root, drive.total_bytes, drive.label):
        raise FormatError("The selected drive changed during confirmation. Run again with the correct card.")

    letter = token[0]
    command = [
        "powershell",
        "-NoProfile",
        "-Command",
        (
            "$ErrorActionPreference='Stop'; "
            f"$partition = Get-Partition -DriveLetter '{letter}'; "
            "if ($partition.IsBoot -or $partition.IsSystem) { throw 'Refusing a boot or system partition' }; "
            f"Format-Volume -DriveLetter '{letter}' -FileSystem FAT32 "
            f"-AllocationUnitSize {ALLOCATION_UNIT_BYTES} -NewFileSystemLabel '{VOLUME_LABEL}' "
            "-Force -Confirm:$false | Out-Null"
        ),
    ]
    try:
        runner(command, check=True)
    except subprocess.CalledProcessError as exc:
        raise FormatError(
            "Windows could not format the SD card. Check the Windows error above, administrator "
            "permissions, write protection, and whether the card supports FAT32 with 512-byte allocation units."
        ) from exc

    verified: DriveInfo | None = None
    for _attempt in range(10):
        try:
            verified = inspect_explicit_drive(target)
        except (OSError, RuntimeError):
            verified = None
        if (
            verified is not None
            and verified.filesystem.upper() == FILESYSTEM
            and verified.allocation_unit_bytes == ALLOCATION_UNIT_BYTES
        ):
            break
        time.sleep(0.5)
    else:
        actual_fs = verified.filesystem if verified is not None else "unavailable"
        actual_unit = verified.allocation_unit_bytes if verified is not None else "unavailable"
        raise FormatError(
            "Formatting finished but verification failed: "
            f"filesystem={actual_fs}, allocation_unit={actual_unit}."
        )

    print(f"Formatted {verified.root} successfully: FAT32, 512-byte allocation unit.")
    return verified


def main(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(sys.argv[1:] if argv is None else argv)
    try:
        format_card(arguments.drive)
        return 0
    except (RuntimeError, OSError, ValueError, EOFError, KeyboardInterrupt) as exc:
        print(f"Format stopped: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
