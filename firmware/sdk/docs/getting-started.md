English | [简体中文](getting-started_zh.md)

# Embedded C SDK: get started safely (Preview)

Write your own ESP32-S3 `app_main` using `watcher_hw_*`. The independent examples
need no Python SDK, Daemon, official application, server, SD card or model.
Python Applications on your computer are a [separate route](../../../docs/sdk.md).
This snapshot is a development preview, not a stable or fully hardware-accepted release.

## Download the test firmware (development preview)

Use the [hardware-check development preview Release](https://github.com/orulink-ai/WatcheRobot-public/releases/tag/hardware-check-preview-20261030),
not the official stable product release. If assets are unavailable, stop and contact
maintainers rather than substituting a similarly named binary. Follow that Release's source/provenance, hashes and exact asset instructions;
do not assume its binary source equals this snapshot's upstream SHA. Download packages
require **Python 3.10+** and the documented esptool dependency. An application-only
`.bin` must **not** be flashed at `0x0` with a web flasher. Use the complete generated
flash set and its exact offsets/verified package entry, never guessed offsets.

## 1. Obtain and pin the public source

Install Git and [ESP-IDF v6.0.2](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32s3/get-started/index.html)
with ESP32-S3 tools. Activate its exported environment in **every new terminal**
(Linux/macOS: `. /path/to/esp-idf/export.sh`; Windows: use the ESP-IDF PowerShell terminal).
`idf.py --version` must report 6.0.2. Other versions are deliberately rejected.

```sh
git clone https://github.com/orulink-ai/WatcheRobot-public.git
cd WatcheRobot-public
git checkout --detach <PUBLIC_COMMIT>
idf.py -C firmware/sdk/examples/hardware_check build
```

Replace `<PUBLIC_COMMIT>` with the full reviewed **public PR head/merge commit**
from GitHub, not the private upstream SHA. Before merge, fetch the PR explicitly
with `git fetch origin pull/<PR_NUMBER>/head` and use `git checkout --detach FETCH_HEAD`.
Do not type placeholders literally. No private-repository access or Python submodule is needed.
Record `git rev-parse HEAD`, `idf.py --version` and `firmware/sdk/SOURCE.json`.
The latter pins upstream source to `e23bee2a642f6c90753b370cb18c46815e0c8f58`
and records individual file hashes and all recorded public export adaptations.
The public commit identifies this documentation and export; the upstream SHA is provenance, not a public checkout command.

All remaining commands run from **WatcheRobot-public repository root**, in the
activated IDF terminal. Expected build result: success and
`firmware/sdk/examples/hardware_check/build/watcher_hw_hardware_check.bin` plus
IDF-generated bootloader/partition/flash arguments. A successful build proves no physical behavior.

## 2. Choose an example

| Goal | Project | Application entry |
| --- | --- | --- |
| Complete guided check | `hardware_check` | `firmware/sdk/examples/hardware_check/main/app_main.c` |
| Body only | `body` | `firmware/sdk/examples/body/main/app_main.c` |
| Camera/audio/display | `head` | `firmware/sdk/examples/head/main/app_main.c` |
| Combined custom application | `robot` | `firmware/sdk/examples/robot/main/app_main.c` |

Replace the final project name in `idf.py -C firmware/sdk/examples/robot build` as needed.
Target is ESP32-S3 with 16 MB flash; head projects need octal PSRAM. Each project
has its own partitions, defaults and pinned component manifests. `sdk.cmake`
selects only hardware dependencies; `media_lib_sal` is included for head audio,
not for body-only use. Registry downloads still require network access.

Edit application code, not the official firmware. A separate consumer project can
include this checkout's `firmware/sdk/examples/sdk.cmake` before `project()`, set
`HW_BODY`/`HW_HEAD`, call `watcher_hw_check_idf()` after `project()`, declare selected
`watcher_hw_*` dependencies and reuse the matching manifest/defaults/partitions.
Always keep the full SDK checkout pinned; copying headers alone is insufficient.

## 3. Prepare the paired device — before flashing ESP32

Record board revision, STM32 firmware version/commit, Himax PTL image SHA256 and
all recovery asset hashes. [Historical evidence](../VALIDATION.md) is not current
pair acceptance. The STM32 must advertise motion/LED/touch capabilities; the camera
requires matching **PTL** Himax firmware. Public product Releases are **not guaranteed**
to contain a validated SDK-compatible pair. If a verified pair or recovery bundle is
missing, **stop before flashing** and request the maintainer's pinned compatibility record.
Do not infer compatibility from a successful HELLO, build, filename or Latest label.

Use the public [official flashing guide](../../../docs/flashing.md) only for
first preparation or restoration with an explicitly verified complete bundle.
It is not the everyday custom-app flash path. Never run an internal workspace or
private-source command. The preview does not supply an unverified replacement STM32/Himax image.

## Official storage overwrite warning

The ESP32 connector is **SERIAL-B** (verify the actual enumerated port).
The first full custom `flash` overwrites the official `agentcfg` region
**0x12000–0x20000**, not merely a hypothetical data-loss risk. Back up required
configuration/data before proceeding. Returning to official firmware requires the
**complete official restore**, not just an application bin or `app-flash`; verify
startup and pairing again. A backup alone does not restore the official partition layout.

## 4. Flash the custom ESP32 application

Back up required data first, clear the full mechanism travel and keep manual power
disconnection available. Identify the **ESP32** serial port in Device Manager or
`/dev/serial/by-id/`; do not use the STM32/Himax port. Close other serial monitors.
Replace `<PORT>` explicitly (for example `COM7` or `/dev/ttyACM0`, only if verified on your machine).

```sh
idf.py -C firmware/sdk/examples/hardware_check -p <PORT> flash
idf.py -C firmware/sdk/examples/hardware_check -p <PORT> monitor
```

Expected: esptool verifies writes; the display shows **Hardware check**, and the log
identifies `Hardware check v2 | curved zones`. Exit monitor with **Ctrl+]**.
These commands write the custom ESP32 bootloader, partition table and application,
not STM32/Himax and not the official app. Different partitions can destroy official
storage even without erasing the whole chip. Do **not** run `erase-flash`.
Use `idf.py -C firmware/sdk/examples/hardware_check -p <PORT> app-flash` only after
confirming that the **connected device** already has the same custom partition layout;
local files alone cannot establish that. Unknown layout or intervening official firmware requires a full custom `flash`.

Equivalent convenience entries (same root and IDF environment):

```sh
bash firmware/sdk/examples/hardware-sdk.sh hardware_check build
bash firmware/sdk/examples/hardware-sdk.sh hardware_check flash <PORT>
```

```powershell
./firmware/sdk/examples/hardware-sdk.ps1 hardware_check build
./firmware/sdk/examples/hardware-sdk.ps1 hardware_check flash <PORT>
```

Both accept body/head/robot/hardware_check, build/flash/monitor and an explicit port
for flash/monitor; bad input/environment returns 2, otherwise IDF's exit code is retained.
No port scanning, paired flashing or erase action is hidden in these entries.

## 5. Observe, recover and migrate

Follow the [26-item hardware check](hardware-check.md). Current servos have **no
physical position feedback**. Command coordinates and ACK/DONE are software evidence,
not measured position or physical stopping. `watcher_hw_body_request_position()`
returns `ESP_ERR_NOT_SUPPORTED`; old feedback-relative examples are disabled.
A safety confirmation may command X90/Y120 from an unknown actual position: this
can be a large movement, not merely a three-degree nudge. See [open-loop contract](servo-open-loop.md).

Disconnected ESP32 cannot guarantee stop delivery. STM32 watchdog/safety and an
operator-controlled power cut remain necessary. Body-event overflow latches FAULT:
cancel pending work, dispatch outside callbacks, then explicitly close/init and
resubscribe; wait for a fresh READY and new user intent. FIFO drain or reconnect
alone does not unlock controls. Audio read/write/drain belongs to one owner task,
not ISR, MCU/camera callbacks or an LVGL lock. Copy borrowed JPEGs and lock LVGL.

For official recovery, stop the custom application, disconnect monitors, select a
known complete **official** Release bundle and follow the public [full restore procedure](../../../docs/flashing.md#full)
for its exact ports/images. This replaces your custom ESP32 app and may replace
storage and paired firmware. Verify official startup and pairing afterward;
rebuild/reflash the custom project if you return to SDK work. Missing recovery
assets or unknown image provenance is a blocker, not a reason to guess offsets.

From the previous namespace, migrate includes/functions/types/CMake dependencies
from `watche_hw_*` to `watcher_hw_*`, constants to `WATCHER_HW_*` and the IDF check to
`watcher_hw_check_idf()`; there are no aliases. Build fresh with
`idf.py -C firmware/sdk/examples/robot -B build-watcher build`.
Never relabel an old binary. [Migration](../MIGRATION.md) also covers event-loss changes.
Historical names/hashes in validation records are retained intentionally, not newly accepted.

## Troubleshooting and acceptance boundary

- IDF version error: activate 6.0.2 in this terminal; do not bypass the check.
- Missing component: verify complete pinned checkout, network registry access and manifest; do not include official `firmware/s3`.
- Body not READY: check power, cable, version/capability negotiation and quiet-link timeout; do not force movement.
- Camera fails: verify PTL pair, then retry/observe timeout; no image means no pass.
- No sound: use Test speaker → voice/tone, start at output 90 (cap 95); progress completion is not audibility.
- Check results are RAM-only and reset on reboot. Save operator observations externally.

Software/host/site tests do not certify motion limits, stop timing, edge touch,
JPEG quality, acoustic quality, repeated teardown or long-duration combined operation.
Full paired-device and official-restore acceptance remains pending; see [validation](../VALIDATION.md).
This work does not merge, deploy production, flash hardware or publish a stable Release.
