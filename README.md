English | [简体中文](README_zh.md)

# WatcheRobot

## Project Preview

![WatcheRobot desktop robot](docs/images/watcher-robot-render.png)

WatcheRobot supports expressions, movement, and audio/video interaction through a desktop client, mobile app, or Python SDK. This repository contains hardware files, mechanical models, the SDK, and guides. Download firmware and installers from [Releases](https://github.com/orulink-ai/WatcheRobot-public/releases).

## Quick Start

### 1. Get the Repository

```bash
git clone --recurse-submodules https://github.com/orulink-ai/WatcheRobot-public.git
cd WatcheRobot-public
```

If you already cloned it but the SDK folder is empty, run `git submodule update --init --recursive`.

### 2. Prepare the Environment and Materials

See the [OSHW project](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl) for the materials list and assembly instructions.

The main parts are the Watcher head, STM32 body board, servos, structural parts, power supply, and SD card. Refer to the OSHW project for the complete list.

| Prepare | Purpose |
| --- | --- |
| ST-LINK V2 | Flash STM32 on the body board |
| USB data cable | Flash Himax and ESP32-S3 in the Watcher head |
| Dedicated Conda environment | Script prepares Python dependencies and flashing tools without using existing Python environments |
| SD card and card reader | Write expression and action resources |
| Computer or phone | Use Desktop, App, or SDK |

Environment setup and copyable commands are all in the flashing guide.

### 3. Flash and Prepare for Startup

Follow the [Flashing Guide](docs/flashing.md) to prepare the body board, head, and SD card in order, then power on. It includes wiring photos and copyable commands.

### 4. Connect and Use the Robot

Choose a client from the [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest):

| Interface | Download |
| --- | --- |
| Windows desktop | x64 setup `.exe` |
| macOS desktop | Apple Silicon `.dmg` |
| Android app | `.apk` |
| iOS app | [TestFlight](https://testflight.apple.com/join/XFCFsm5M) |
| Python SDK | `watcherobot` `.whl`; follow the [SDK Guide](docs/sdk.md) |

See the [network setup instructions (Chinese PDF)](docs/manuals/device-network-setup.pdf). Once connected, try an expression and a movement to check the display, lights, and motion.

## Guides and Files

- [Flashing Guide](docs/flashing.md): wiring, environment, flashing, SD card, and startup checks.
- [SDK Guide](docs/sdk.md): installation, pairing, and running Python scripts.
- [Hardware Files](hardware/README.md): PCB and mechanical files.
- [Full User Manual (Chinese PDF)](docs/manuals/WatcheRobot-user-manual-20260724.pdf).

Firmware source is not published here. The Python SDK is provided through the `python-sdk/` submodule. See [Repository Scope](docs/governance.md) for details.

## License

This repository uses [GPL-3.0](LICENSE). Subprojects and third-party components retain their own licenses.
