<p><strong>English</strong> | <a href="PULL_REQUEST_TEMPLATE_zh.md">简体中文</a></p>

## Summary

-

## Area

- [ ] SDK
- [ ] Hardware
- [ ] Documentation
- [ ] Release process
- [ ] Flashing tools

## Branch and Scope

- Branch name:
- Related issue or checklist:

## Documentation Checklist

- [ ] Quick Start still points to the right path.
- [ ] Flashing instructions are still accurate.
- [ ] SD-card, first-run, and SDK instructions are still accurate.
- [ ] Version-source and compatibility notes were updated if this changes a public version.
- [ ] Hardware/BOM docs were updated if this changes hardware reproduction.
- [ ] Matching English and Chinese documents were updated together.

## Test Plan

- [ ] I ran the relevant SDK tests or documentation checks.
- [ ] I ran `python -m pytest tools/tests -q` and validated the affected target if this changes flashing tools.
- [ ] I reviewed the first-run check if this changes the usage flow.
- [ ] I did not add release binaries to Git.
- [ ] I did not add ESP32-S3 or STM32F103 firmware source code.
- [ ] I checked for secrets, private machine paths, and local-only serial-port notes.

## Notes

-
