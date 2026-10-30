English | [简体中文](README_zh.md)

# ESP32 Embedded Hardware SDK (Development Preview)

Write your own `app_main` with `watcher_hw_*`, independently of the official app,
Python SDK, Daemon, server, SD card and models. This is a hardware-only source
snapshot, **not a stable release or complete paired-device acceptance**.

## Start here

1. [Pin public source, install ESP-IDF 6.0.2 and build](docs/getting-started.md).
2. [Run the 8-group / 26-item hardware check](docs/hardware-check.md).
3. Edit `examples/robot/main/app_main.c`, then build from repository root:
   `idf.py -C firmware/sdk/examples/robot build`.

Flashing changes the ESP32 application/partitions and can overwrite storage.
Verify your ESP32 port and paired STM32/Himax images **before flashing**; setup,
custom flash, official recovery and expected results are in the first guide.
Current servos have **no physical position feedback**. Reference X90/Y120 can
require large movement from an unknown start. ACK/DONE is not a position sensor.

## What do you want to do?

| Goal | Entry |
| --- | --- |
| Check real hardware, safely and one item at a time | [Hardware check](docs/hardware-check.md) |
| Build/flash/restore or use your own project | [Getting started](docs/getting-started.md) |
| Migrate the old namespace | [Migration](MIGRATION.md) |
| Understand open-loop motion | [Current hardware contract](docs/servo-open-loop.md) |
| Inspect public headers | `components/sdk/watcher_hw_{core,body,camera,audio,display}/include/` |
| Understand ownership, locks and event overflow | Headers, [audit](AUDIT.md), [setup](docs/getting-started.md) |
| Review actual evidence, not assumptions | [Historical validation](VALIDATION.md), [current sync evidence](SYNC_VALIDATION.md) |
| Develop on a computer with existing official firmware | [Python SDK](../../docs/sdk.md) — separate route |

`examples/body`, `head`, `robot`, and `hardware_check` are independent ESP-IDF projects.
`examples/common` contains shared UI, pure flow, open-loop body adapter and CC0
speech assets. Do not include the official firmware top-level project.

## Provenance, scope and licensing

[SOURCE.json](SOURCE.json) pins upstream ESP32 commit
`ab4e324e2b15a1be5431c34de106f05d55ccb99e` and inventories exported file hashes.
The public checkout commit is the delivery version; the private upstream is provenance
only, not a user prerequisite. The allowlist exporter is `tools/export_firmware_sdk.py`
in the public repository. Only component-root and unused private bridge include paths
are adapted in build files; documentation adaptations are recorded in the inventory.
No official app/cloud/voice business services, models, credentials or firmware binaries
are exported. Downloaded managed dependencies/build outputs are excluded.

First-party snapshot code retains upstream [Apache-2.0](LICENSE), not the repository
root GPL replacement. Seeed, SSCMA, codec, expander and Espressif files keep their
original copyright/licenses. Registry dependencies retain their own licenses.
This is a delivery snapshot, not an independently maintained fork; fixes should flow
upstream. Historical names/hashes in evidence remain untouched and do not prove
acceptance of this renamed snapshot. Complete physical pairing, safety and restoration
checks remain pending. No stable v1 or production deployment is claimed.
