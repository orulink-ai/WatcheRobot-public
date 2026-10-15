# Building a hardware CI bench with Raspberry Pi

WatcheRobot combines an ESP32-S3 and an STM32F103. Regular CI proves that code builds; hardware-in-the-loop (HIL) flashes real hardware and evaluates boot logs, MCU communication, and observable behavior.

> [!IMPORTANT]
> The HIL bench is under active construction. This page documents the agreed architecture, release baseline, and rollout stages; it does not claim that every hardware test is available today.

## Why Raspberry Pi

The Raspberry Pi connects to ST-LINK, ESP32 debug, and serial adapters over USB. It runs headlessly: the harness reads serial byte streams directly, preserves raw logs, and produces CI reports.

```text
GitHub-hosted runner
  └─ Builds a firmware artifact
                 │
                 ▼
Raspberry Pi HIL node
  ├─ Flashes ESP32
  ├─ Flashes STM32
  ├─ Captures both serial streams
  ├─ Controls reset and power
  └─ Reports PASS / FAIL
```

## Current release baseline

The first stage validates the bench with formal release assets:

| Controller | Release | Current validation boundary |
|---|---|---|
| ESP32-S3 | `v0.4.2` | Download, integrity verification, flashing, and boot checks |
| STM32F103 | `v0.1.2` | The release has not yet received ST-LINK hardware acceptance; HIL will close that gap |

The `WatcheRobot_hil` repository pins this baseline explicitly. It never drifts to a newer release without review.

## Where the code lives

The HIL controller has its own GitHub repository and is referenced by `WatcheRobot-Workspace` as a submodule. GitHub is the source of truth; the Raspberry Pi checkout is replaceable.

During development, the Pi uses a regular checkout under `~/projects/WatcheRobot_hil`. Firmware caches, bench-specific configuration, and raw logs stay outside version control. Once the bench is stable, an immutable runtime can be installed under `/opt/watcher-hil` so unfinished development cannot affect CI.

## From releases to pull requests

Without a firmware PR, the bench downloads the pinned ESP32 and STM32 releases, verifies their SHA-256 checksums, and runs baseline checks. Later, a firmware PR will build its exact commit on a GitHub-hosted runner and pass that artifact to the same HIL harness.

```text
Now:   Release → download → verify → flash → hardware test
Later: PR commit → build artifact → flash → hardware test → PR status
```

## Rollout stages

1. Repeatable environment checks, release downloads, and checksum verification.
2. Stable ESP32, STM32, and ST-LINK identity plus automated flashing.
3. Timestamped dual-UART capture and boot/communication smoke tests.
4. Controlled power, motor, and encoder closed-loop tests.
5. Non-blocking PR reporting followed by a required check after the bench proves stable.

The HIL job should not block merges until it can reliably distinguish a product failure from a bench infrastructure failure.

