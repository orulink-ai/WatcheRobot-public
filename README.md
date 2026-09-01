<div align="center">

<p><strong>English</strong> | <a href="README_zh.md">简体中文</a></p>

<img src="docs/images/watcher-robot-render.png" alt="WatcheRobot render" width="720">

<p>Open-source materials for the WatcheRobot desktop robot, focused on hardware reproduction, mechanical reference, SDK integration, firmware flashing, and device validation.</p>

<p>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL--3.0-blue.svg" alt="License: GPL-3.0"></a>
  <img src="https://img.shields.io/badge/Package-0.1.0-brightgreen" alt="Package 0.1.0">
  <img src="https://img.shields.io/badge/Firmware-Release%20Assets-green" alt="Firmware release assets">
  <img src="https://img.shields.io/badge/Hardware-Gerber%20%7C%20BOM%20%7C%20CPL-orange" alt="Hardware: Gerber, BOM, CPL">
  <img src="https://img.shields.io/badge/SDK-Python-blue" alt="Python SDK">
</p>

</div>

---

## Overview

WatcheRobot is a desktop interaction robot built around SenseCAP Watcher, an ESP32-S3, an STM32F103 co-processor, custom PCBs, mechanical parts, and SD-card behavior assets. This repository publishes the materials needed for reproduction and integration, including hardware manufacturing files, mechanical models, the Python SDK, flashing tools, user manuals, and release-asset documentation.

The open-source boundary has changed: this repository does not publish the ESP32-S3 or STM32F103 embedded firmware source code. Firmware is distributed as same-version prebuilt GitHub Release assets, so users can flash the device, prepare SD-card assets, and validate startup without accessing firmware source.

Through the Python SDK, developers can control the WatcheRobot ESP32-S3 from the host side and use the camera, microphone, speaker, and built-in expressions and animation effects. For interfaces and examples, see the [Python SDK documentation](python-sdk/README.md).

## Quick Start

This section helps you get the repository, prepare tools, and complete the smallest verifiable startup flow.

### 1. Get the Repository

```bash
git clone https://github.com/orulink-ai/WatcheRobot.git
cd WatcheRobot
```

If you plan to submit changes, fork the repository first and create a branch from your fork.

### 2. Prepare Tools

| Purpose | Tools |
| --- | --- |
| Base environment | Git, Python 3.11+ |
| ESP32-S3 flashing | USB serial driver, repository flashing tool, or AI flashing skill from the Release |
| STM32F103 flashing | ST-LINK or compatible SWD tool, with the same-version Release firmware package |
| SD-card assets | FAT32 SD card and the same-version SD-card asset package |
| Hardware validation | Serial tool, multimeter, or basic hardware debugging tools |

### 3. Get Firmware and Behavior Assets

Prefer downloading all runtime assets from the same [GitHub Release](https://github.com/orulink-ai/WatcheRobot/releases) bundle. First-time reproduction needs at least the ESP32-S3 firmware, STM32F103 firmware, and SD-card assets from the same version; the Release may also include an AI flashing skill ZIP that an AI assistant can read before helping with flashing.

The current `watche-v0.1.1` asset list is documented in [Downloads](docs/downloads.md).

### 4. Flash Firmware and Prepare the SD Card

After downloading Release assets, flash STM32F103, flash ESP32-S3, and copy the SD-card assets before running first-start validation. The concrete steps are documented in [Firmware: Flashing and Assets](firmware/README.md#flashing-and-assets).

If you want an AI assistant to help with flashing, download `WatcheRobot-Flashing-Skill-v0.1.1.zip` from the Release or ask the AI to read [WatcheRobot Firmware Flashing Skill](tools/flashing/README.md). The skill guides the AI through same-version asset selection, serial-port detection, flashing, and boot-log checks.

### 5. First-Start Validation

Use the [first-start validation checklist](docs/action-test.md) to check:

- ESP32-S3 boot logs
- SD-card asset recognition
- one basic servo or actuator action
- one LED or display behavior
- basic status returned from serial, BLE, WebSocket, or SDK entrypoints

After these checks pass, the smallest hardware, firmware, and resource chain is working.

## Repository Layout

```text
firmware/
  README.md       Firmware Release assets, flashing entry, and resource notes

hardware/
  README.md       Hardware package map and BOM notes
  pcb/            PCB source, schematics, layout PDFs, Gerber, BOM, CPL files, and spare-parts template
  3d-models/      Mechanical model exports
  assembly/       Assembly images or documents when available

python-sdk/
  README.md       Python SDK source, examples, and tests

docs/
  flashing.md             Firmware flashing and tool guide
  manuals/                Device network setup and full user manual PDFs
  sd-card-assets.md       SD-card behavior asset guide
  behavior-flash-skill.md Behavior asset checklist
  action-test.md          First-start validation checklist
  sdk.md                  SDK and public interface boundary
  versions.md             Version source-of-truth guide
  downloads.md            Release asset guide
  compatibility.md        Version compatibility matrix
  release-process.md      Release process and asset rules
  governance.md           Repository boundary notes

tools/
  ...             Flashing, release, and resource helper tools
```

## Project Documentation

- [Firmware and flashing notes](firmware/README.md)
- [Flashing tool guide](docs/flashing.md)
- [SD-card behavior assets](docs/sd-card-assets.md)
- [Behavior asset field checklist](docs/behavior-flash-skill.md)
- [First-start validation](docs/action-test.md)
- For device network setup, see the [device network setup guide](docs/manuals/device-network-setup.pdf).
- For the complete usage guide, see the [WatcheRobot user manual](docs/manuals/WatcheRobot-user-manual-20260724.pdf).
- [SDK and public interface boundary](docs/sdk.md)
- [Python SDK](python-sdk/README.md)
- [Version tracking](docs/versions.md)
- [Contribution guide](CONTRIBUTING.md)
- [Security policy](SECURITY.md)

## Public Scope

Open in this repository:

- Python SDK source
- PCB and mechanical publication files
- firmware flashing, resource preparation, and release helper tools
- user manuals, network setup guide, and release documentation

Not published as source in this repository:

- ESP32-S3 firmware source
- STM32F103 firmware source
- Android app source
- server source
- desktop app source

Distributed through GitHub Releases when available:

- ESP32-S3 prebuilt firmware
- STM32F103 prebuilt firmware
- SD-card asset package
- Android app package
- desktop app installer
- manifest and checksum files

See [docs/downloads.md](docs/downloads.md) for the expected release asset types and current release status.

## Tech Stack

| Area | Main Contents |
| --- | --- |
| Python SDK | Python package, examples, host-side tests |
| Hardware | EasyEDA Pro, Gerber, BOM, CPL, STEP |
| Firmware assets | ESP32-S3 and STM32F103 prebuilt Release packages |
| Flashing tools | Python, Windows helper scripts, AI flashing skill |

## License

This repository is licensed under [GPL-3.0](LICENSE), unless a subproject or third-party component states otherwise in its own license file.
