<p><strong>English</strong> | <a href="CONTRIBUTING_zh.md">简体中文</a></p>

# Contributing

Thank you for helping improve WatcheRobot. This repository publishes hardware materials, mechanical models, the Python SDK, user documentation, flashing tools, and release-asset documentation.

## Forks and Branches

1. Fork `orulink-ai/WatcheRobot-public`.
2. Clone your fork.
3. Create a focused working branch.

Recommended branch names:

| Work type | Branch format |
| --- | --- |
| Documentation | `docs/<topic>` |
| SDK | `sdk/<topic>` |
| Hardware materials | `hardware/<topic>` |
| Release docs or manifests | `release/<topic>` |
| Tools | `tools/<topic>` |

Keep each pull request focused on one clear change.

## PR Titles

Use short prefixes:

- `docs: ...`
- `sdk: ...`
- `hardware: ...`
- `release: ...`
- `tools: ...`

Examples:

- `docs: add SD-card behavior asset checklist`
- `hardware: document spare parts for wireless charging base`
- `sdk: add camera capture example`

## Do Not Commit

Do not commit:

- ESP32-S3 or STM32F103 firmware source code
- Release artifacts: `.bin`, `.zip`, `.exe`, `.msi`, `.dmg`, `.apk`, `.aab`
- local build outputs or generated firmware images
- Wi-Fi credentials, API keys, tokens, private keys, or `.env` files
- local machine paths, private serial logs, or bench-only COM-port notes
- closed-source App, Server, or Desktop source code

Binary release artifacts should be uploaded to GitHub Releases instead of entering Git history.

## Documentation Updates

If your change affects setup, flashing, behavior assets, hardware materials, SDK usage, or Release assets, update the matching documents:

- Quick start: `README.md`
- Firmware and flashing: `firmware/README.md`, `docs/flashing.md`
- SD card and first run: `docs/sd-card-assets.md`, `docs/action-test.md`
- SDK: `docs/sdk.md`
- Versions and compatibility: `docs/versions.md`, `docs/compatibility.md`
- Hardware and BOM: `hardware/README.md`, `hardware/pcb/spares.md`

Update the matching Chinese document at the same time.

## Validation Before PR

Run checks according to the change scope:

- Documentation-only changes: check language switches and local links.
- SDK changes: run `python -m pytest` under `python-sdk/`.
- Flashing-tool changes: run `python -m pytest tools/tests -q` and validate the affected target when applicable.
- Usage-flow changes: review `docs/action-test.md`.
- Hardware material changes: confirm BOM, CPL, Gerber, schematic, layout, and editable source filenames correspond to the same board.

If your environment is missing a tool, state that clearly in the PR Test Plan.
