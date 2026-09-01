# WatcheRobot Firmware Flashing Skill

This README is intended for AI-assisted flashing workflows. Users should not need to understand every script argument manually; when a user asks to flash WatcheRobot firmware, the AI assistant should read this file first, then operate based on the local machine, serial ports, and Release assets.

## User Prompt Example

```text
Please read tools/flashing/README.md and help me flash WatcheRobot.
Use the latest Release ESP32-S3 firmware, STM32F103 firmware, and SD-card assets, detect the current serial port, and check boot logs after flashing.
```

## Inputs to Confirm

- Current repository path.
- Target hardware: ESP32-S3, STM32F103, SD-card assets, or the full bundle.
- Release version. If unspecified, prefer the latest WatcheRobot Release.
- Local serial port. If unspecified, list ports and use plug/unplug changes to identify the board.
- Whether boot-log monitoring is needed after flashing.
- SD-card drive letter or mount path.

## Recommended Flow

1. Check the Git worktree and avoid committing Release ZIPs, logs, or temporary files.
2. Download assets from the same GitHub Release bundle; do not mix versions.
3. Verify `SHA256SUMS.txt` or the Release manifest.
4. Identify the ESP32-S3 serial port; ask the user to replug the device if needed.
5. Flash the ESP32-S3 Release ZIP with the repository flashing helper.
6. If the user asks for STM32F103, use the STM32 release package from the same Release and the user's current debug-probe tooling.
7. If the user asks for SD-card assets, copy the same-version SD-card asset package to the target SD card.
8. Capture boot logs and report success, failure, or required manual checks.

## Tool Entrypoints

- `tools/flash-release.cmd`: Windows shortcut for ESP32-S3 Release ZIP flashing.
- `python -m tools.win_flasher`: ESP32-S3 Release ZIP flashing, serial-port listing, and log monitoring.
- `docs/flashing.md`: serial drivers, port discovery, and platform differences.
- `docs/sd-card-assets.md`: SD-card asset layout and copy instructions.

## Expected Output

After finishing, the AI assistant should report:

- Release version and asset filenames used.
- Serial port or device alias used.
- Whether the flashing command succeeded.
- Key boot-log result.
- Whether the user still needs to check hardware, power, buttons, or the SD card.
