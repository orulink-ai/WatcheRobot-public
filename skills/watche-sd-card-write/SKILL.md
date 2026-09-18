---
name: watche-sd-card-write
description: Download, verify, and install official WatcheRobot resources to a FAT32 SD card through a card reader using the repository's cross-platform writer.
---

English | [简体中文](SKILL_zh.md)

# WatcheRobot SD-card Writing

Read [the SD-card guide](../../docs/sd-card-assets.md). Use only a card reader and the repository launchers. Require the dedicated non-base Conda environment described in the flashing guide; never install into or alter another Python environment.

Resolve the exact SD-card root before writing. On Windows use `powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd --drive "E:\"`. On macOS/Linux use `bash tools/flash.sh sd --drive "/Volumes/WATCHE"`. Replace the example target with the actual card. Never guess a drive or mount point, and never select a system drive.

Without `--package`, the writer downloads the latest official package. For a Release archive, append `--package <watche-sd-resources-*.tar.gz>`. Do not manually extract the release archive: the writer validates it and maps it into the device's `watche/` layout.

The writer must confirm FAT32, writable media, the package manifest, every file hash, and the final installed layout. It preserves creator works and uses staging before switching the official catalog. Do not format a card, delete unrelated files, or use `--force` unless the user explicitly asks to discard an unfinished device transaction.

Report success only after exit code zero and the final `Installed ... successfully` message. Then instruct the user to eject the card safely and insert it into the powered-off head.
