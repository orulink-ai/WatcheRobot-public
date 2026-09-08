<p><strong>English</strong> | <a href="action-test_zh.md">简体中文</a></p>

# First Action Smoke Test

Use this checklist after flashing firmware and preparing the SD-card behavior assets. The goal is not full validation; it is to prove that the public package can boot and execute one visible behavior.

## Required Setup

- Required attachments downloaded from the latest Release
- STM32F103 firmware from that Release flashed
- Himax and ESP32-S3 flashed with that Release's `PTL-paired` ZIP
- FAT32 SD card prepared from that Release's `sd-resources` archive
- SD card root contains `assets/`, `official_catalog.json`, and `resource_manifest.json`
- Robot connected to power
- Serial monitor available
- BLE tool, WebSocket gateway, or other control path available if testing remote commands

Complete the firmware and SD card preparation before installing a client. For remote validation, use a Desktop, Android, iOS, or Python SDK client distributed by the same Release.

## Boot Checks

- ESP32 serial monitor opens at the expected port.
- Boot log reaches the application startup path.
- Display initializes.
- No missing SD-card resource error appears.
- If STM32 is connected, MCU Link attempts handshake over `USART2 @ 921600 8N1`.
- PTL camera initialization reaches the expected Himax bridge startup path without a firmware-transfer error.

## Behavior Checks

Run the smallest available behavior path for the bench:

| Check | Pass criteria |
| --- | --- |
| Boot animation | Display shows a boot or standby animation. |
| Servo motion | One servo command moves within the expected safe range. |
| LED behavior | One static or breathing LED command changes visible LEDs. |
| Touch event | Touch input produces a log or state change when hardware is present. |
| BLE or WebSocket command | A simple command returns an ack or visible state change. |

If the full robot is not assembled, record which checks were skipped and why.

## Suggested Manual Commands

STM32 local CLI, when connected through `USART1 @ 115200 8N1`:

```text
servo 1 90
ws red
ws off
```

ESP32 behavior and network commands depend on the active app, BLE, or WebSocket bench. Keep command logs only when they do not contain Wi-Fi credentials, private paths, or local-only serial-port notes.

## Known Limitations

- This smoke test does not replace CI.
- Cross-end protocol contract coverage is incomplete and tracked in GitHub issue #5.
- This checklist applies only to the components distributed together in the selected Release.
- Record hardware reflash, camera, video, audio, orientation, and continuous-run checks as unverified until they are performed on the target hardware.
