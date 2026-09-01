# First Action Smoke Test

Use this checklist after flashing firmware and preparing the SD-card behavior assets. The goal is not full validation; it is to prove that the public package can boot and execute one visible behavior.

## Required Setup

- ESP32-S3 firmware flashed from the matching Release package
- SD card prepared with `anim/anim_manifest.bin` and `.animpack` files
- Robot connected to power
- Serial monitor available
- BLE tool, WebSocket gateway, or other control path available if testing remote commands

## Boot Checks

- ESP32 serial monitor opens at the expected port.
- Boot log reaches the application startup path.
- Display initializes.
- No missing SD-card `anim/` asset error appears.
- If STM32 is connected, MCU Link attempts handshake over `USART2 @ 921600 8N1`.

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

ESP32 behavior and network commands depend on the active app, BLE, or WebSocket bench. Keep command logs in the PR or release notes only if they do not contain Wi-Fi credentials, private paths, or local-only serial-port notes.

## Known Limitations

- This smoke test does not replace CI.
- Cross-end protocol contract coverage is incomplete and tracked in GitHub issue #5.
- If a matching Release package is not available, do not mix older firmware and newer SD-card assets.
