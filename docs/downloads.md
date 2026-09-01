# Downloads

All release artifacts are published through GitHub Releases for `orulink-ai/WatcheRobot-public`.

## Current Status

Release assets are published through GitHub Releases when available. If a matching release package is not available, wait for the matching bundle or ask the maintainers for the correct package; do not mix assets from different versions.

- `README.md`
- `firmware/README.md`
- `docs/flashing.md`
- `docs/sd-card-assets.md`

## Current Bundle

The current `watche-v0.1.1` bundle should provide these files together:

| Content | File |
| --- | --- |
| ESP32-S3 firmware | `WatcheRobot-ESP32S3-v0.3.2.zip` |
| STM32F103 firmware | `WatcheRobot-STM32F103-v0.1.1.zip` |
| SD-card assets | `WatcheRobot-SDCard-Assets-v0.3.2.zip` |
| AI flashing skill | `WatcheRobot-Flashing-Skill-v0.1.1.zip` |
| Windows desktop | `Watcher.Desktop_0.2.10_x64-setup.exe` |
| macOS desktop | `Watcher.Desktop_0.2.10_aarch64.dmg` |
| Android app | `watcher-android-0.3.8.apk` |
| Manifest and checksums | `WatcheRobot-Bundle-v0.1.1.manifest.json`, `SHA256SUMS.txt` |

For hardware reproduction, use the ESP32-S3 firmware, STM32F103 firmware, and SD-card assets from the same Release bundle.

## Required Assets for a Public Release

A public release should include:

- ESP32 firmware flash ZIP
- ESP32 SD-card behavior asset ZIP
- STM32 firmware package when available
- hardware package ZIP when available
- `SHA256SUMS.txt`
- release notes
- compatibility notes linked to `docs/compatibility.md`

Full product releases may additionally include:

- Android app APK
- server Windows package
- desktop Windows installer

Do not download app, server, or desktop source code from this repository; those components are distributed only as release artifacts.

## Artifact Rules

- Do not commit `.apk`, `.aab`, `.exe`, `.msi`, `.dmg`, `.zip`, `.bin`, `.elf`, `.map`, or `.hex` files to Git.
- Upload release artifacts to GitHub Releases.
- Record checksums in `SHA256SUMS.txt`.
- Update `docs/versions.md` and `docs/compatibility.md` for every public release.
