English | [简体中文](embedded-sdk_zh.md)

# Embedded C SDK (Development Preview)

## Pick your development route

| Route | Where your code runs | Start here |
| --- | --- | --- |
| Python SDK | Computer-side Applications, existing firmware and Runtime/Daemon | [Python quick start](sdk.md) |
| Embedded C SDK | ESP32-S3, your own `app_main`, UI and hardware policy | [Public source and setup](../firmware/sdk/README.md) |

These are independent routes, not prerequisites for each other. The embedded
examples need no Python SDK, official app, Daemon, server, SD card or models.

## First steps

1. [Pin a public commit and install ESP-IDF 6.0.2](../firmware/sdk/docs/getting-started.md).
2. Build `hardware_check`, then verify paired firmware/recovery and your ESP32 port
   before any flash. Build commands run from the **public repository root**.
3. [Follow the 8-group / 26-item check](../firmware/sdk/docs/hardware-check.md), then
   edit body/head/robot application code for your own product.

The public snapshot includes `watcher_hw_{core,body,camera,audio,display}`, their
hardware-only dependencies, common UI/flow helpers, licensed test speech, four
independent projects and Shell/PowerShell entries. Headers specify ownership and
threading; [migration](../firmware/sdk/MIGRATION.md) covers the old namespace.
The full immutable upstream SHA and per-file provenance are in
[SOURCE.json](../firmware/sdk/SOURCE.json). No private checkout is needed.

## Current hardware and release boundary

Current servos have **no physical position feedback**. Position queries return
`ESP_ERR_NOT_SUPPORTED`; command coordinates and ACK/DONE are not measurements.
Motion tests explicitly confirm full-travel safety before commanding X90/Y120,
which can be a large move from an unknown start. STOP still requires physical
observation and STM32-side safety. [Read before moving](../firmware/sdk/docs/hardware-check.md).

This is a **development preview**, not a stable Release, complete STM32/Himax
pairing, factory certificate or mechanical-safety guarantee. Camera requires PTL
Himax firmware; public official Releases must not be assumed compatible. Missing
verified pair or recovery images blocks flashing. [Setup/custom flash/official
recovery](../firmware/sdk/docs/getting-started.md) are separate workflows.

[Historical records](../firmware/sdk/VALIDATION.md) retain original names, hashes
and limitations. [This sync's software evidence](../firmware/sdk/SYNC_VALIDATION.md)
does not establish new audible, visual or mechanical acceptance. No production
publication or stable v1 is performed by this source/documentation change.
