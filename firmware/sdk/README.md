English | [简体中文](README_zh.md)

# ESP32 Embedded Hardware SDK (Development Preview)

This directory contains `watche_hw_*` source, required drivers and three independent
ESP-IDF projects. Developers own `app_main`, without the official app, Python SDK,
Daemon, server, SD card or models. Full hardware acceptance is pending; this is not
a stable release.

## Getting started

1. Download this repository or run `git clone https://github.com/orulink-ai/WatcheRobot-public.git`.
2. Install ESP-IDF **6.0.2** with ESP32-S3 tools and activate its terminal environment.
3. Edit `firmware/sdk/examples/robot/main/app_main.c`: this is your application.
4. From the repository root run `idf.py -C firmware/sdk/examples/robot build`.
5. Verify the port and paired firmware, back up required data, then run
   `idf.py -C firmware/sdk/examples/robot -p <PORT> flash monitor`.

Replace `<PORT>` with the actual serial port. Target: ESP32-S3, 16MB flash and
octal PSRAM for head examples. A successful build is not hardware safety evidence.
Long presses in body examples may trigger small movements; clear the mechanism first.

## Examples and modules

- `examples/body`: motion, lights, body touch. No motion without fresh valid position feedback.
- `examples/head`: LVGL and screen touch, JPEG capture, short PCM recording/playback.
- `examples/robot`: combined application; recommended starting point.
- `components/sdk/watche_hw_*`: public headers; other components are required dependencies.

Examples own their CMake, partitions and configuration. They never include the
official application project. Headers document parameters, ownership and threading.
Applications dispatch body callbacks with `dispatch_events`; submission is not motion
completion. Commands are rejected before readiness; reconnection never replays motion.
JPEG bytes are borrowed during callbacks; copy to retain. Blocking audio belongs in
application tasks. Lock LVGL access; never record or close display while holding its lock.

## Using a separate project

Keep a pinned checkout of this repository beside your project. In your top-level
CMake, set `HW_BODY` / `HW_HEAD` and include this SDKs `examples/sdk.cmake` before
`project()`, then call `watche_hw_check_idf()`. It finds components relative to itself,
not a private repository. Declare selected `watche_hw_*` main-component dependencies
and reuse the matching example manifest, partitions and sdkconfig.defaults.
Disable `HW_HEAD` for body-only projects. Do not ship products against floating main.

## Scripts and flashing

Windows: `./firmware/sdk/examples/hardware-sdk.ps1 robot build`.
macOS/Linux: `bash firmware/sdk/examples/hardware-sdk.sh robot build`.
Both accept example (default robot), action (default build), port. Actions are
build/flash/monitor; flash and monitor require an explicit port.

See the [official flashing guide](../../docs/flashing.md) for initial STM32/Himax
preparation and restoring official firmware. Official tools replace custom ESP32
applications; do not use them for everyday SDK flashing. Camera requires paired
PTL Himax firmware; current Releases are not guaranteed to contain it. Do not flash
unknown pairs before checking [acceptance](VALIDATION.md). First flash writes the
bootloader and partition table and may overwrite storage; do not erase flash.
Use `app-flash` for later application updates only after confirming unchanged partitions.
Disconnect safety depends on STM32 mechanisms, not guaranteed ESP32 stop delivery.

## Licensing and maintenance

First-party source in this directory retains the upstream ESP32 [Apache-2.0](LICENSE)
license; the repository-root GPL-3.0 does not replace it. Seeed BSP, SSCMA, expander
and Espressif codec directories retain original licenses and copyright notices.
Registry dependencies keep their respective licenses. This is a public delivery
snapshot; the internal ESP32 repository remains the development source. Fixes must
flow back upstream, not create an independently maintained duplicate. Official
application and STM32/Himax implementations remain outside this publication scope.
