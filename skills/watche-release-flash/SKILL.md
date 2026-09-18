---
name: watche-release-flash
description: Prepare tools automatically and flash released WatcheRobot STM32 or paired Himax/ESP32 firmware through the repository launchers.
---

English | [简体中文](SKILL_zh.md)

# Released Firmware Flashing

Use the repository's `tools/flash.ps1` on Windows or `tools/flash.sh` on macOS/Linux. Follow the guide to create and activate a dedicated named Conda environment (example: `watcherobot`). If that name already exists, create a different new name; never reuse an existing environment without user approval. The scripts reject base and mismatched interpreters and prepare dependencies and flashing tools. Never install dependencies into base, user-site, or another existing Python environment. Do not change global PATH or Conda configuration. Missing Conda is a prerequisite, not a reason to fall back to system Python.

Read [the guide](../../docs/flashing.md). Resolve the extracted firmware folder and the authorized target. For STM32, pass `stm32 --package <folder-containing-bin>`. For the head, pass `head --package <paired-folder> --port <control> --vision-port <vision>`. Resolve paths relative to the repository; quote paths with spaces.

For head flashing, SERIAL-B / MI_02 is the ESP32 control port passed to `--port`; SERIAL-A / MI_00 is the Himax port passed to `--vision-port`. First run the head command without ports and use the script's three-column mapping to obtain the arguments; use the guide's read-only port query when a second check is useful. Confirm both belong to the same CH342 USB serial number. The launcher checks the A/B roles; if they are reversed, stop and use the reported mapping. The paired tool writes Himax before ESP32. Do not alter firmware binaries or replace the selected release.

For the initial full installation of the public Release, append `--factory` so the ESP32 partition table, application, and storage all come from the same package. This overwrites existing ESP32 data. If an existing device must preserve data, stop and explain the distinction; do not silently turn an update request into a full installation.

`--prepare-only` downloads and checks tools without accessing hardware. It is not hardware acceptance. Windows driver installation may require administrator privileges; preserve OS prompts and stop on failure. Driver download failure must not be reported as successful preparation. CH342 driver auto-install and Linux permission setup are not yet implemented; report these limitations if encountered.

For STM32, require exit code zero and OpenOCD's Programming Finished, Verified OK, and Resetting Target messages. For the head, require the Himax completion, PTL paired completion, and repository-entry completion messages in sequence. Never report simulations or environment preparation as a successful flash. Install SD resources only through a card reader and the repository writer.

Perform whole-device acceptance only after reinserting the SD card and powering on: reach the normal interface, open Phone Control, connect the phone over BLE, play one SD-backed expression, and run one small movement. The first Phone Control entry may need a few seconds for BLE and the STM32 link to initialize. If any check fails, report the verified stage precisely; a successful write is not by itself a successful whole-device acceptance.
