<p><strong>English</strong> | <a href="governance_zh.md">简体中文</a></p>

# Governance

`orulink-ai/WatcheRobot-public` is the public repository for hardware materials, mechanical models, SDK source, user documentation, flashing tools, and release coordination.

Maintainers should keep the following boundaries clear:

- Python SDK source is provided through the `python-sdk/` submodule.
- Hardware source changes belong under `hardware/pcb/`, `hardware/3d-models/`, or `hardware/assembly/`.
- The C SDK is a pinned `c-sdk/` submodule of the public [WatcheRobot_c_sdk](https://github.com/orulink-ai/WatcheRobot_c_sdk) repository, not a duplicated source snapshot. Its own licenses and third-party attribution apply.
- Official ESP32 application, STM32F103 and Himax implementations are not published in this repository.
- App, server, desktop, firmware, and SD-card runtime artifacts are release assets only.
- Release artifacts are uploaded to GitHub Releases, not committed to Git.
- Private implementation details do not belong in public docs.

Branch protection should require CI and owner review before public release.
