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
- all required downloadable artifacts

A public release must include at least the ESP32 firmware flash ZIP, SD-card behavior asset ZIP, `SHA256SUMS.txt`, release notes, and compatibility notes. Large binaries, firmware ZIPs, APKs, installers, DMGs, Gerber ZIPs, and hardware package ZIPs must be uploaded as release assets.
