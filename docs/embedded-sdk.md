English | [简体中文](embedded-sdk_zh.md)

# Independent Embedded Hardware SDK: Development Preview

## Current Status

This document describes direction and interface boundaries. Development-preview
source, independent examples and build instructions are in
[firmware/sdk](../firmware/sdk/README.md). There is no stable release or complete
hardware-validated STM32/Himax pairing yet; do not treat the preview as stable.

The available development entry remains the [Python SDK](sdk.md). Existing
firmware and client assets are listed in
[Releases](https://github.com/orulink-ai/WatcheRobot-public/releases).

## Two Development Paths

| Path | Runs on | Developer ownership | Status |
| --- | --- | --- | --- |
| Python SDK | Host computer | Applications using existing device firmware | Public entry available; see the SDK guide |
| Embedded hardware SDK | ESP32-S3 | Own app_main, UI, interactions, and hardware calls | Development preview; not publicly released |

The embedded SDK aims to run independently of the official application, without
the Python SDK, Daemon, or server. It does not change the existing
Python SDK/Daemon/Application architecture.

## First-release Target Capabilities

| Module | Target interfaces |
| --- | --- |
| Body motion | Target angles in degrees, duration, stop; completion, rejection, and fault events |
| Body lights | Zone colors, brightness, and basic effects |
| Body touch | Ordered press, release, and long-press events |
| Camera | PTL backend; single capture, continuous JPEG frames, stop, and status |
| Microphone and speaker | PCM capture, playback, volume, and format configuration |
| Display and screen touch | Standard LCD/LVGL/touch handles and access protection |

Modules are optional components named `watche_hw_*`; developers own application
behavior. Voice assistants, cloud connections, official expressions, AI inference,
and model management are outside the first release. The target toolchain is
ESP-IDF **6.0.2** and ESP32-S3, with required dependencies pinned.

The runtime owns MCU handshaking, event collection, state transitions, and disconnect
handling. Applications subscribe to events. Successful motion submission does not
mean completion; commands must be rejected before readiness, and reconnects must
not replay old motion. Callbacks handle lightweight events; expensive work belongs
in application tasks. Published SDK documentation must specify resource contracts,
including JPEG buffer lifetime and display locking.

## Independent Example Targets

- Body: touch changes lights, user-triggered small motion, completion/fault logging.
- Head: simple LVGL page, screen touch, JPEG capture, short PCM recording/playback.
- Whole robot: combine these capabilities and demonstrate resource ownership.

Basic examples require neither an SD card nor model resources. Developers edit
example application code without starting the official top-level firmware project
or modifying STM32/Himax firmware.

## Flashing and Safety Boundaries

The final delivery must distinguish first-time STM32/Himax preparation, everyday
custom ESP32 flashing, and restoration of official whole-robot firmware.

The current [Flashing Guide](flashing.md) covers official firmware. Existing tools
may write the official ESP32 application and must not be used to preserve a custom
application. Custom partition layouts may overwrite storage; the released guide
must document backup and restoration. Until the independent development entry is
released, this preview provides no unverified custom flashing commands.

After disconnection, ESP32 cannot guarantee delivery of a stop command; safety
depends on STM32-side mechanisms. Motion examples require limit, small-movement,
and stop validation. A successful build is not evidence of motion safety.

## Public Release Gates

- Confirm public source delivery, licensing, and a fixed Git revision; update
  the published open-source scope accordingly.
- Validate installation, all three builds, and application-only edits from a clean checkout.
- Run Windows/Linux host tests and pinned-IDF CI, including official firmware regression.
- Validate motion, lights, all three body-touch events, JPEG decoding, PCM recording/
  playback, screen touch, and combined operation on hardware. Missing or incompatible
  modules must not hang the device or unintentionally drive motors.
- Record SDK/STM32 versions, Himax image checksum, board revision, and acceptance evidence.
- Validate custom flashing and official restoration, with equivalent PowerShell/Shell entries.

Do not release version one until both head and body pass acceptance. This document
does not promise a release date.

[Back to the project](../README.md)
