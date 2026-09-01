from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import serial
from rich.console import Console

from .models import ParsedFlashPackage


class FlashingError(RuntimeError):
    """Raised when flashing or monitoring fails."""


def ensure_esptool_installed() -> None:
    if importlib.util.find_spec("esptool") is None:
        raise FlashingError(
            "未安装 esptool。请先执行: python -m pip install -r tools\\win_flasher\\requirements.txt"
        )


def _write_segments_to_tempdir(package: ParsedFlashPackage, tempdir: Path) -> list[tuple[int, Path]]:
    extracted: list[tuple[int, Path]] = []
    for segment in package.segments:
        file_name = f"{segment.offset:08x}-{segment.file_name}"
        target_path = tempdir / file_name
        target_path.write_bytes(segment.data)
        extracted.append((segment.offset, target_path))
    return extracted


def build_esptool_command(
    package: ParsedFlashPackage,
    port: str,
    baud: int,
    extracted_segments: list[tuple[int, Path]],
) -> list[str]:
    command = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        package.chip,
        "-p",
        port,
        "-b",
        str(baud),
        "--before",
        "default_reset",
        "--after",
        "hard_reset",
        "write_flash",
        "--flash_mode",
        package.flash_mode,
        "--flash_freq",
        package.flash_freq,
        "--flash_size",
        package.flash_size,
    ]

    for offset, path in extracted_segments:
        command.extend([hex(offset), str(path)])
    return command


def flash_package(package: ParsedFlashPackage, port: str, baud: int, console: Console) -> None:
    ensure_esptool_installed()
    with tempfile.TemporaryDirectory(prefix="watche-flash-") as temp_dir:
        temp_path = Path(temp_dir)
        extracted_segments = _write_segments_to_tempdir(package, temp_path)
        command = build_esptool_command(package, port, baud, extracted_segments)

        console.print(f"[cyan]Running:[/cyan] {' '.join(command)}")
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        assert process.stdout is not None
        for line in process.stdout:
            console.print(line.rstrip())

        return_code = process.wait()
        if return_code != 0:
            raise FlashingError(f"esptool 退出码非 0: {return_code}")


def monitor_port(
    port: str,
    baud: int,
    seconds: int,
    console: Console,
) -> None:
    try:
        with serial.Serial(port=port, baudrate=baud, timeout=0.2) as serial_port:
            console.print(
                f"[cyan]Monitoring {port} at {baud} baud for {seconds} seconds. Press Ctrl+C to stop.[/cyan]"
            )
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                try:
                    line = serial_port.readline()
                except serial.SerialException as exc:
                    raise FlashingError(f"串口读取失败: {exc}") from exc

                if not line:
                    continue

                try:
                    console.print(line.decode("utf-8", errors="replace").rstrip())
                except UnicodeDecodeError:
                    console.print(repr(line))
    except serial.SerialException as exc:
        raise FlashingError(f"无法打开串口 {port}: {exc}") from exc
