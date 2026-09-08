English | [简体中文](README_zh.md)

# WatcheRobot

The public WatcheRobot desktop-robot repository provides hardware files, mechanical models, the Python SDK, flashing tools, user guides, and Release asset documentation.

## Project Preview

![WatcheRobot desktop robot](docs/images/watcher-robot-render.png)

WatcheRobot is designed for desktop companionship, interactive demos, and developer experiments. It combines a Watcher head, ESP32-S3, an STM32 body board, servos, mechanical parts, and SD-card resources. You can use it through Desktop, the mobile App, or the Python SDK.

## Quick Start

### 1. Get the Repository

```bash
git clone --recurse-submodules https://github.com/orulink-ai/WatcheRobot-public.git
cd WatcheRobot-public
```

If `python-sdk/` is empty in an existing clone, run:

```bash
git submodule update --init --recursive
```

### 2. Prepare the Environment and Materials

See the [OSHW project](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl) for the bill of materials and assembly steps.

| Prepare | Purpose |
| --- | --- |
| Assembled Watcher head and robot body | Flash and run the robot |
| ST-LINK V2 | Flash STM32 on the body board |
| USB data cable | Flash Himax and ESP32-S3 in the head |
| SD card and card reader | Write expression, movement, and other resources |
| Conda | Create an isolated environment; the scripts prepare the remaining dependencies and tools |
| Computer or phone | Use Desktop, App, or SDK |

### 3. Prepare the Robot

Open the [Flashing Guide](docs/flashing.md) and follow its sections in order:

1. Flash STM32 on the body board.
2. Flash the Watcher head; the script writes Himax first and ESP32-S3 second.
3. Use a card reader to extract the SD resource archive to the SD-card root, then return the card to the head.
4. Power on and complete the startup check.

### 4. Connect and Use

Download the required client or SDK from the [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest):

| Interface | Download |
| --- | --- |
| Windows Desktop | x64 `.exe` installer |
| macOS Desktop | Apple Silicon `.dmg` installer |
| Android App | `.apk` installer |
| iOS App | [TestFlight](https://testflight.apple.com/join/XFCFsm5M) |
| Python SDK | `watcherobot` `.whl`; follow the [SDK Guide](docs/sdk.md) |

See the [device network setup guide (Chinese PDF)](docs/manuals/device-network-setup.pdf). Once connected, run one expression and one movement to check the display, lights, and motion.

## Repository Structure

```text
WatcheRobot-public/
├─ README.md / README_zh.md       English and Chinese project entry
├─ docs/
│  ├─ flashing.md / flashing_zh.md        Flashing guide
│  ├─ sd-card-assets.md / sd-card-assets_zh.md    SD-card resource guide
│  ├─ action-test.md / action-test_zh.md          First-run check
│  ├─ sdk.md / sdk_zh.md                  SDK guide
│  ├─ versions.md / versions_zh.md        Version sources
│  ├─ compatibility.md / compatibility_zh.md      Compatibility notes
│  ├─ release-process.md / release-process_zh.md  Release rules
│  ├─ governance.md / governance_zh.md            Repository rules
│  ├─ images/                              Documentation images
│  └─ manuals/                             PDF user manuals
├─ firmware/README.md / README_zh.md       Firmware download entry
├─ hardware/
│  ├─ pcb/
│  │  ├─ schematic/                        Schematics
│  │  ├─ layout/                           PCB layouts
│  │  ├─ gerber/                           Production files
│  │  ├─ bom/ / cpl/                      BOM and placement files
│  │  └─ pcb-source/                       EasyEDA Pro project source
│  └─ 3d-models/exports/                   STEP mechanical models
├─ python-sdk/                             Python SDK submodule
├─ skills/watche-release-flash/
│  ├─ SKILL.md                             English flashing Skill
│  └─ SKILL_zh.md                          Chinese flashing Skill
├─ tools/
│  ├─ flash.ps1                            Windows flashing entry
│  ├─ flash.sh                             macOS/Linux flashing entry
│  └─ flash_setup.py                       Dependency setup and flashing dispatcher
├─ .github/                                Issue, PR, and CI configuration
├─ CONTRIBUTING.md / CONTRIBUTING_zh.md    Contribution guide
├─ SECURITY.md / SECURITY_zh.md            Security policy
└─ LICENSE                                 Open-source license
```

## Project Documentation

- [Flashing Guide](docs/flashing.md): environment, wiring, firmware flashing, SD card, and startup checks.
- [Firmware Entry Point](firmware/README.md): download package and flashing entry for each target.
- [SD-card Resources](docs/sd-card-assets.md) and [First-run Check](docs/action-test.md).
- [SDK Guide](docs/sdk.md): installation, pairing, and running Python scripts.
- [Hardware Files](hardware/README.md): PCB and mechanical-file entry point.
- [Full User Manual (Chinese PDF)](docs/manuals/WatcheRobot-user-manual-20260724.pdf).
- [Version Sources](docs/versions.md), [Compatibility](docs/compatibility.md), [Release Rules](docs/release-process.md), and [Repository Rules](docs/governance.md).
- [Contributing Guide](CONTRIBUTING.md) and [Security Policy](SECURITY.md).

## Open-source Scope

- This repository publishes hardware production files, mechanical models, the Python SDK, flashing tools, and documentation.
- Python SDK source is provided through the `python-sdk/` submodule.
- ESP32-S3, Himax, STM32, App, server, and Desktop source code is not published here.
- Firmware, client installers, and SD-card resources are distributed through GitHub Releases.

## Technology Stack

| Area | Technology or format |
| --- | --- |
| Head | Watcher, Himax, ESP32-S3 |
| Body control | STM32F103, SWD / ST-LINK |
| SDK and flashing tools | Python, PowerShell, Bash |
| Hardware files | Schematic, PCB, BOM, CPL, Gerber, STEP |

## License

This repository uses [GPL-3.0](LICENSE). Subprojects and third-party components retain their own licenses.
