<div align="center">

<p><strong>English</strong> | <a href="README_zh.md">简体中文</a></p>

</div>

# Firmware

Firmware source is not included in this repository. Download the STM32 ZIP, ESP32-S3 + Himax `PTL-paired` ZIP, SD-card resource archive from the release marked **Latest** on [GitHub Releases](https://github.com/orulink-ai/WatcheRobot-public/releases).

After assembly, use this order:

1. Check wiring and power polarity while powered off.
2. Flash STM32F103 through ST-LINK/SWD.
3. Follow the Skill inside the paired ZIP to flash Himax, then ESP32-S3.
4. Prepare and insert the FAT32 SD card.
5. Power on and run the first-start checks.

See the [Firmware Flashing Guide](../docs/flashing.md) for the entrypoints and the [Downloads Guide](../docs/downloads.md) for asset selection.
