<div align="center">

<p><strong>English</strong> | <a href="README_zh.md">简体中文</a></p>

</div>

# Firmware

`firmware/` is the entrypoint for WatcheRobot firmware flashing and resource preparation. This repository does not publish ESP32-S3 or STM32F103 firmware source code; firmware is distributed as prebuilt GitHub Release assets.

## Flashing and Assets

For first-time reproduction, download these assets from the same [GitHub Release](https://github.com/orulink-ai/WatcheRobot/releases) bundle:

| Content | File |
| --- | --- |
| ESP32-S3 firmware | `WatcheRobot-ESP32S3-v0.3.2.zip` |
| STM32F103 firmware | `WatcheRobot-STM32F103-v0.1.1.zip` |
| SD-card assets | `WatcheRobot-SDCard-Assets-v0.3.2.zip` |
| AI flashing skill | `WatcheRobot-Flashing-Skill-v0.1.1.zip` |
| Manifest and checksums | `WatcheRobot-Bundle-v0.1.1.manifest.json`, `SHA256SUMS.txt` |

Do not mix firmware and SD-card assets from different releases. The complete asset list is maintained in [Downloads](../docs/downloads.md).

### AI-Assisted Flashing

The repository provides an AI-oriented flashing skill: [WatcheRobot Firmware Flashing Skill](../tools/flashing/README.md). The Release may also include the same skill as `WatcheRobot-Flashing-Skill-v0.1.1.zip`. If you use Codex or another AI coding assistant, ask it to read this README or the README inside the ZIP, then let it handle Release asset selection, serial-port detection, ESP32-S3 flashing, SD-card asset preparation, and boot-log checks.

You can say:

```text
Please read tools/flashing/README.md and help me flash WatcheRobot.
Use the latest Release ESP32-S3 firmware, STM32F103 firmware, and SD-card assets, detect the current serial port, and check boot logs after flashing.
```

### ESP32-S3 Release ZIP Flashing

```bash
python -m pip install -r tools/win_flasher/requirements.txt
python -m tools.win_flasher list-ports
python -m tools.win_flasher flash --zip .\WatcheRobot-ESP32S3-v0.3.2.zip --port COM7 --monitor
```

On Windows, you can also use:

```powershell
tools\flash-release.cmd --zip .\WatcheRobot-ESP32S3-v0.3.2.zip --port COM7 --monitor
```

### STM32F103 Firmware Flashing

STM32F103 flashing depends on your physical debug probe and bench setup. This public repository does not provide an STM32 source build entrypoint; use `WatcheRobot-STM32F103-v0.1.1.zip` from the Release as the flashing input.

Minimum hardware expectations:

- MCU: `STM32F103C8Tx`
- Debug probe: ST-LINK or compatible SWD tool
- Local debug UART: `USART1 @ 115200 8N1`
- ESP32 co-processor link: `USART2 @ 921600 8N1`

### SD-Card Assets

SD-card asset layout and copy instructions are documented in [SD Card Assets](../docs/sd-card-assets.md). Driver notes, port discovery, and platform differences are documented in [Firmware Flashing Guide](../docs/flashing.md).

## Release

Prebuilt firmware, flashing bundles, and SD-card animation asset packages should not be committed directly to this Git repository. Upload them as GitHub Release assets.

Release notes should state the compatible App, Server, Desktop, hardware, and model versions.
