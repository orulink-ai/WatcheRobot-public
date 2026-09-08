<p><strong>English</strong> | <a href="release-process_zh.md">简体中文</a></p>

# Release Process

Release artifacts are managed through GitHub Releases, not committed to Git.

Recommended tag names:

- full product set: `watche-vX.Y.Z`
- firmware only: `firmware-vX.Y.Z`
- hardware only: `hardware-vX.Y.Z`
- Android app: `app-vX.Y.Z`
- server package: `server-vX.Y.Z`
- desktop installer: `desktop-vX.Y.Z`

Each release should include:

- release notes
- compatibility matrix
- version notes from `docs/versions.md`
- `SHA256SUMS.txt`
- a machine-readable bundle manifest
- all required downloadable artifacts

A full public test release includes the ESP32-S3 + Himax PTL paired ZIP, STM32 ZIP, device SD-card `.tar.gz`, Python SDK wheel, Windows x64 installer, macOS aarch64 DMG, Android APK, bundle manifest, and `SHA256SUMS.txt`. Its iOS build is distributed through the TestFlight entry in the release notes. Standalone ESP32-S3, standalone Himax, and separate Skill packages are unnecessary when the PTL paired ZIP already contains both binaries, cross-platform tools, and the paired Skill.

Large binaries, firmware packages, APKs, installers, DMGs, resource archives, Gerber ZIPs, and hardware package ZIPs must be uploaded as release assets rather than committed to Git.
