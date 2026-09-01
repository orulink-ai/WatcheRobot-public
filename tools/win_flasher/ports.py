from __future__ import annotations

from dataclasses import dataclass

from serial.tools import list_ports


@dataclass(frozen=True)
class PortInfo:
    device: str
    description: str
    hwid: str
    vid: int | None
    pid: int | None


def list_serial_ports() -> list[PortInfo]:
    ports = [
        PortInfo(
            device=port.device,
            description=port.description or "",
            hwid=port.hwid or "",
            vid=port.vid,
            pid=port.pid,
        )
        for port in list_ports.comports()
    ]

    def sort_key(port: PortInfo) -> tuple[int, str]:
        suffix = port.device.replace("COM", "")
        if suffix.isdigit():
            return (int(suffix), port.device)
        return (9999, port.device)

    return sorted(ports, key=sort_key)
