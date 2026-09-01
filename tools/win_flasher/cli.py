from __future__ import annotations

import argparse
from pathlib import Path

from rich.console import Console
from rich.panel import Panel
from rich.prompt import Confirm, IntPrompt, Prompt
from rich.table import Table

from .flasher import FlashingError, flash_package, monitor_port
from .package_parser import PackageParseError, parse_flash_package
from .ports import PortInfo, list_serial_ports
from .releases import get_latest_release, get_repo_root, scan_release_entries

console = Console()


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m tools.win_flasher",
        description="WatcheRobot Windows release flasher",
    )
    subparsers = parser.add_subparsers(dest="command")

    subparsers.add_parser("list-releases", help="List discovered release ZIP packages")
    subparsers.add_parser("list-ports", help="List available Windows serial ports")

    flash_parser = subparsers.add_parser("flash", help="Flash a release ZIP")
    flash_parser.add_argument("--zip", dest="zip_path", help="Release ZIP path")
    flash_parser.add_argument("--port", help="Serial port, e.g. COM7")
    flash_parser.add_argument("--baud", type=int, default=460800, help="Flash baud rate")
    flash_parser.add_argument("--monitor", action="store_true", help="Open short serial monitor after flashing")
    flash_parser.add_argument(
        "--monitor-seconds", type=int, default=15, help="Monitor duration in seconds"
    )
    flash_parser.add_argument(
        "--monitor-baud", type=int, default=115200, help="Monitor baud rate"
    )
    return parser


def _show_release_table(entries: list, title: str = "Release ZIPs") -> None:
    table = Table(title=title)
    table.add_column("#", justify="right")
    table.add_column("Version")
    table.add_column("ZIP")
    table.add_column("Path")
    for index, entry in enumerate(entries, start=1):
        table.add_row(str(index), entry.version, entry.zip_name, str(entry.zip_path))
    console.print(table)


def _show_ports_table(ports: list[PortInfo], title: str = "Serial Ports") -> None:
    table = Table(title=title)
    table.add_column("#", justify="right")
    table.add_column("Port")
    table.add_column("Description")
    table.add_column("VID:PID")
    table.add_column("HWID")
    for index, port in enumerate(ports, start=1):
        vid_pid = ""
        if port.vid is not None and port.pid is not None:
            vid_pid = f"{port.vid:04X}:{port.pid:04X}"
        table.add_row(str(index), port.device, port.description, vid_pid, port.hwid)
    console.print(table)


def _choose_release_zip() -> Path:
    entries = scan_release_entries()
    latest = entries[0] if entries else None

    if latest is None:
        manual_path = Prompt.ask("No scanned release ZIP found. Enter a ZIP path")
        return Path(manual_path).expanduser().resolve()

    _show_release_table(entries)
    console.print(f"[green]Latest release:[/green] {latest.version} -> {latest.zip_path}")
    console.print("1. Use latest release ZIP")
    console.print("2. Choose another scanned release ZIP")
    console.print("3. Enter a custom ZIP path")
    choice = IntPrompt.ask("Select ZIP source", choices=["1", "2", "3"], default=1)

    if choice == 1:
        return latest.zip_path
    if choice == 2:
        selected = IntPrompt.ask("Enter release number", choices=[str(i) for i in range(1, len(entries) + 1)])
        return entries[selected - 1].zip_path

    manual_path = Prompt.ask("Enter ZIP path")
    return Path(manual_path).expanduser().resolve()


def _choose_port() -> str:
    ports = list_serial_ports()
    if not ports:
        return Prompt.ask("No serial ports detected. Enter a COM port manually")

    _show_ports_table(ports)
    if len(ports) == 1:
        default_port = ports[0].device
        if Confirm.ask(f"Use the only detected port {default_port}?", default=True):
            return default_port
        return Prompt.ask("Enter a COM port manually", default=default_port)

    selection_choices = [str(i) for i in range(1, len(ports) + 1)]
    selected = IntPrompt.ask("Select COM port", choices=selection_choices, default=1)
    return ports[selected - 1].device


def _summarize_package(package) -> None:
    table = Table(title="Flash Layout")
    table.add_column("Offset")
    table.add_column("File")
    table.add_column("Size")
    for segment in package.segments:
        table.add_row(hex(segment.offset), segment.file_name, str(len(segment.data)))

    console.print(
        Panel.fit(
            f"ZIP: {package.package_path}\n"
            f"Version: {package.version or 'unknown'}\n"
            f"Chip: {package.chip}\n"
            f"Flash mode: {package.flash_mode}\n"
            f"Flash freq: {package.flash_freq}\n"
            f"Flash size: {package.flash_size}",
            title="Package Summary",
        )
    )
    console.print(table)


def _auto_port_for_noninteractive() -> str:
    ports = list_serial_ports()
    if len(ports) == 1:
        return ports[0].device
    if not ports:
        raise FlashingError("未检测到可用串口，请使用 --port 显式指定。")
    raise FlashingError("检测到多个串口，请使用 --port 显式指定 COM 口。")


def run_interactive() -> int:
    console.print(Panel.fit("WatcheRobot Windows Release Flasher", style="bold cyan"))
    try:
        zip_path = _choose_release_zip()
        package = parse_flash_package(zip_path)
        _summarize_package(package)
        port = _choose_port()
        baud = IntPrompt.ask("Flash baud", default=460800)
        enable_monitor = Confirm.ask("Open serial monitor after flashing?", default=False)
        monitor_seconds = 15
        if enable_monitor:
            monitor_seconds = IntPrompt.ask("Monitor duration (seconds)", default=15)

        if not Confirm.ask(f"Flash {zip_path.name} to {port}?", default=True):
            console.print("[yellow]Cancelled.[/yellow]")
            return 0

        flash_package(package, port=port, baud=baud, console=console)
        console.print("[green]Flashing completed successfully.[/green]")
        if enable_monitor:
            monitor_port(port=port, baud=115200, seconds=monitor_seconds, console=console)
        return 0
    except (PackageParseError, FlashingError, OSError) as exc:
        console.print(f"[red]Error:[/red] {exc}")
        return 1
    except KeyboardInterrupt:
        console.print("\n[yellow]Cancelled by user.[/yellow]")
        return 130


def command_list_releases() -> int:
    entries = scan_release_entries()
    if not entries:
        console.print(
            "[yellow]No local release ZIP packages found. Download ZIP assets from GitHub Releases "
            "or place them under .local/release-zips for scanning.[/yellow]"
        )
        return 1
    _show_release_table(entries)
    console.print(f"[green]Latest:[/green] {entries[0].version} -> {entries[0].zip_path}")
    return 0


def command_list_ports() -> int:
    ports = list_serial_ports()
    if not ports:
        console.print("[yellow]No serial ports detected.[/yellow]")
        return 1
    _show_ports_table(ports)
    return 0


def command_flash(args: argparse.Namespace) -> int:
    try:
        if args.zip_path:
            zip_path = Path(args.zip_path).expanduser().resolve()
        else:
            latest = get_latest_release(get_repo_root())
            if latest is None:
                raise FlashingError("未扫描到 release ZIP，请使用 --zip 显式指定。")
            zip_path = latest.zip_path

        package = parse_flash_package(zip_path)
        port = args.port or _auto_port_for_noninteractive()
        _summarize_package(package)
        flash_package(package, port=port, baud=args.baud, console=console)
        console.print("[green]Flashing completed successfully.[/green]")
        if args.monitor:
            monitor_port(port=port, baud=args.monitor_baud, seconds=args.monitor_seconds, console=console)
        return 0
    except (PackageParseError, FlashingError, OSError) as exc:
        console.print(f"[red]Error:[/red] {exc}")
        return 1


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    if args.command == "list-releases":
        return command_list_releases()
    if args.command == "list-ports":
        return command_list_ports()
    if args.command == "flash":
        return command_flash(args)
    return run_interactive()
