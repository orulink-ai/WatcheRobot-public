# Governance

`orulink-ai/WatcheRobot-public` is the public repository for hardware materials, mechanical models, SDK source, user documentation, flashing tools, and release coordination.

Maintainers should keep the following boundaries clear:

- SDK source changes belong under `python-sdk/`.
- Hardware source changes belong under `hardware/pcb/`, `hardware/3d-models/`, or `hardware/assembly/`.
- ESP32-S3 and STM32F103 firmware source code is not published in this repository.
- App, server, desktop, firmware, and SD-card runtime artifacts are release assets only.
- Release artifacts are uploaded to GitHub Releases, not committed to Git.
- Private implementation details do not belong in public docs.

Branch protection should require CI and owner review before public release.
