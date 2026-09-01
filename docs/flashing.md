# Firmware Flashing Guide

This guide is the public flashing entry for WatcheRobot firmware. Firmware source code is not included in this repository; use prebuilt firmware and SD-card assets from the same GitHub Release bundle.

## Tool Matrix

| Target | Required tools | Notes |
| --- | --- | --- |
| ESP32-S3 release ZIP | Python 3.11+, USB serial driver, `esptool`, `pyserial`, `rich` | Install with `python -m pip install -r tools/win_flasher/requirements.txt`. |
| STM32F103 firmware package | ST-LINK or compatible SWD probe, vendor flashing tool | Use the firmware package from the same Release bundle. |
| SD-card assets | FAT32 SD card and a file extraction tool | Copy the released `anim/` assets to the card root. |

## Serial Drivers and Ports

Windows:

- Install the USB serial driver required by the board or USB-UART adapter.
- Open Device Manager and confirm the `COMx` port.
- Use explicit ports in examples, such as `--port COM7`.

macOS:

- Install a driver only if your adapter does not appear automatically.
- Ports usually appear as `/dev/cu.usbserial-*` or `/dev/cu.usbmodem*`.
- Use `ls /dev/cu.*` to list candidate ports.

Linux:

- Ports usually appear as `/dev/ttyUSB*` or `/dev/ttyACM*`.
- Add your user to `dialout` or the equivalent serial group if permission is denied.
- Re-login after changing group membership.

## ESP32-S3 Release ZIP Flashing

When a GitHub Release contains an ESP32 firmware flash ZIP, use the helper:

```bash
python -m pip install -r tools/win_flasher/requirements.txt
python -m tools.win_flasher list-ports
python -m tools.win_flasher flash --zip .\WatcheRobot-ESP32S3-v0.3.2.zip --port COM7 --monitor
```

The helper expects a ZIP that contains `flash_args.txt`, `bootloader.bin`, `partition-table.bin`, and the app firmware image.

For interactive Windows flashing, you can also run:

```powershell
tools\flash-release.cmd --zip .\WatcheRobot-ESP32S3-v0.3.2.zip --port COM7 --monitor
```

## STM32F103 Board Flashing

Board flashing depends on the physical probe and bench setup.

Minimum public expectations:

- MCU target: `STM32F103C8Tx`
- Debug probe: ST-LINK or compatible SWD probe
- Local debug UART: `USART1 @ 115200 8N1`
- ESP32 co-processor link: `USART2 @ 921600 8N1`

Use the STM32 firmware package from the same Release bundle as the ESP32 firmware and SD-card assets.

## AI-Assisted Flashing

For repeated or device-specific flashing, let the AI assistant read [WatcheRobot Firmware Flashing Skill](../tools/flashing/README.md) first. The skill explains how to choose same-version assets, detect ports, run flashing tools, and check boot logs.

## Common Problems

| Symptom | Check |
| --- | --- |
| Port not found | Replug USB, check driver, confirm the OS-specific port name. |
| Permission denied on Linux | Add user to `dialout` or run with a temporary udev rule. |
| Flash ZIP rejected | Confirm the ZIP contains `flash_args.txt` and required binaries. |
| STM32 flashing fails | Confirm SWD wiring, probe driver, target power, and the firmware package version. |
| Monitor shows unreadable text | Confirm baud rate and target port. |
