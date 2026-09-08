English | [简体中文](README_zh.md)

# Firmware Entry Point

ESP32-S3, Himax, and STM32 firmware source is not published in this repository. Flashable firmware is distributed through the [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest).

| Target | Download from the Release | Method |
| --- | --- | --- |
| STM32 body board | STM32 firmware package | Connect ST-LINK V2 to the body board and run the repository script |
| Watcher head | Paired Himax + ESP32-S3 package | Connect the head by USB and run the repository script |
| SD card | SD resource archive | Extract it to the SD-card root with a card reader |

See the [Flashing Guide](../docs/flashing.md) for environment, wiring, and commands; [SD-card Resources](../docs/sd-card-assets.md) for card contents; and the [First-run Check](../docs/action-test.md) after startup.

Codex users can also ask the assistant to read [`skills/watche-release-flash/SKILL.md`](../skills/watche-release-flash/SKILL.md) and follow it for flashing.
