---
name: watche-sd-card-write
description: Download, verify, and install official WatcheRobot resources to a FAT32 SD card through a card reader using the repository's cross-platform writer.
---

English | [简体中文](SKILL_zh.md)

# WatcheRobot SD-card Writing

Read [the SD-card guide](../../docs/sd-card-assets.md). Use only a card reader and the repository launchers. Require the dedicated non-base Conda environment described in the flashing guide; never install into or alter another Python environment.

Resolve the exact SD-card root before writing. On Windows use `powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd --drive "E:\"`. On macOS/Linux use `bash tools/flash.sh sd --drive "/Volumes/WATCHE"`. Replace the example target with the actual card. Never guess a drive or mount point, and never select a system drive.

Without `--package`, the writer downloads the latest official package. For a Release archive, append `--package <watche-sd-resources-*.tar.gz>`. Do not manually extract the release archive: the writer validates it and maps it into the device's `watche/` layout.

The card must use FAT32 with a 512-byte allocation unit. Windows users can select these settings in the card's Format dialog, or run `powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd-format --drive "E:\"` in an Administrator terminal with the dedicated Conda environment active. Formatting deletes all files, including resources and creator works: back them up first. Only format an identified card when the user authorizes formatting; a resource update alone does not authorize it. The command formats immediately without an additional confirmation prompt; check the target before running it. The formatter supports Windows removable drives only.

The writer must confirm FAT32, writable media, the package manifest, every file hash, and the final installed layout. Windows also checks the allocation unit; on macOS/Linux, confirm the 512-byte setting when preparing the card. Resource writing preserves creator works and uses staging before switching the official catalog. Do not delete unrelated files. Use `--force` only when the user explicitly asks to discard an unfinished device transaction.

Report card-writing success only after exit code zero and the final `Installed ... successfully` message. Then instruct the user to eject the card safely and insert it into the powered-off head. Device-side acceptance additionally requires a normal boot and one SD-backed expression to play; do not describe a successful computer-side write as proof that the robot read the card.
