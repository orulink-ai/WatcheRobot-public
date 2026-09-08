<p><strong>English</strong> | <a href="versions_zh.md">简体中文</a></p>

# Release Tracking

Product version numbers change with every publication and are intentionally not duplicated in the repository documentation. The release marked **Latest** on [GitHub Releases](https://github.com/orulink-ai/WatcheRobot-public/releases), its notes, bundle manifest, and checksums are the source of truth.

## Sources of Truth

| Component | Current public source | How to identify it |
| --- | --- | --- |
| WatcheRobot bundle | Latest GitHub Release | Release tag and `WatcheRobot-Bundle-*.manifest.json` |
| Repository source snapshot | Git commit | Commit SHA on the default branch |
| Python SDK | Git submodule and Release wheel | `python-sdk/` and the `watcherobot` `.whl` asset |
| ESP32-S3 + Himax PTL firmware | Release asset | ZIP containing `S3` and `PTL-paired` |
| Device SD card resources | Release asset | `.tar.gz` archive containing `sd-resources` |
| STM32F103 firmware | Release asset | ZIP containing `STM32` |
| Windows/macOS desktop | Release assets | Windows x64 installer and macOS Apple Silicon DMG |
| Android app | Release asset | APK |
| iOS app | Release notes | TestFlight link |
| Hardware | Repository files | Current files under `hardware/` and the linked OSHW project |

## Publication Rules

- Record all component versions only in the GitHub Release notes and bundle manifest.
- Keep repository instructions version-neutral and select assets from the release marked **Latest**.
- Update the compatibility notes when requirements, asset types, hardware support, or installation order changes.
- Include `SHA256SUMS.txt` and a machine-readable bundle manifest with every full release.
- Do not infer functional hardware acceptance from package-integrity verification.
