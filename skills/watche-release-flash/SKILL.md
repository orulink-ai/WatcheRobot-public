---
name: watche-release-flash
description: Prepare tools automatically and flash released WatcheRobot STM32 or paired Himax/ESP32 firmware through the repository launchers.
---

English | [简体中文](SKILL_zh.md)

# Released Firmware Flashing

Use the repository's `tools/flash.ps1` on Windows or `tools/flash.sh` on macOS/Linux. Follow the guide to create and activate a dedicated named Conda environment (example: `watcherobot`). If that name already exists, create a different new name; never reuse an existing environment without user approval. The scripts reject base and mismatched interpreters and prepare dependencies and flashing tools. Never install dependencies into base, user-site, or another existing Python environment. Do not change global PATH or Conda configuration. Missing Conda is a prerequisite, not a reason to fall back to system Python.

Read [the guide](../../docs/flashing.md). Resolve the extracted firmware folder and the authorized target. For STM32, pass `stm32 --package <folder-containing-bin>`. For the head, pass `head --package <paired-folder> --port <control> --vision-port <vision>`. Resolve paths relative to the repository; quote paths with spaces.

For head flashing, SERIAL-B / MI_02 is control and SERIAL-A / MI_00 is vision. Confirm both belong to the same CH342 USB serial number. The paired tool writes Himax before ESP32. Do not alter firmware binaries, replace the selected release, or use factory erase as a workaround.

`--prepare-only` downloads and checks tools without accessing hardware. It is not hardware acceptance. Windows driver installation may require administrator privileges; preserve OS prompts and stop on failure. Driver download failure must not be reported as successful preparation. CH342 driver auto-install and Linux permission setup are not yet implemented; report these limitations if encountered.

For STM32, require exit code zero and OpenOCD's Programming Finished, Verified OK, and Resetting Target messages. For head flashing require successful paired completion and check startup separately. Never report simulations or environment preparation as a successful flash. SD resources are copied through a card reader only.
