English | [简体中文](hardware-check_zh.md)

# Hardware check: 8 groups, 26 manual observations

This independent `hardware_check` ESP32 firmware checks one real item at a time.
It is not factory certification or an emergency-stop device. Start with the
[pinned public setup, build, flash and recovery guide](getting-started.md).
No Python SDK, Daemon, server, SD card, model or private repository is needed.
Build/flash from the public repository root, with ESP-IDF 6.0.2 activated and a
verified ESP32 `<PORT>` (not the Himax/STM32 port):

```sh
idf.py -C firmware/sdk/examples/hardware_check build
idf.py -C firmware/sdk/examples/hardware_check -p <PORT> flash
idf.py -C firmware/sdk/examples/hardware_check -p <PORT> monitor
```

Expected startup log: `Hardware check v2 | curved zones`; expected screen:
**Hardware check**. Exit monitor with Ctrl+]. No test runs automatically.

## Operator flow

Tap **Test** to enter an item, read its instruction, then **Test** to run it.
Observe before choosing **Next** (accept this item) or **Not OK** (record issue).
On an issue page, **Back** retries; **Next** keeps the issue and continues.
A missing/skipped item never passes. The two pale curved lower regions are the
main touch targets, not just their text labels; the central gap and outside arc
are inactive. The last report has two pages of four groups. Tap a group/item to
retest. A new session asks before clearing results; reboot clears all RAM results.

| Group | Actual action | What the operator must confirm |
| --- | --- | --- |
| Screen | Tap 1 → 2 → 3 | Readable display and actual touch responses |
| Camera | 3/2/1 photo, then five-second live view | Fresh image and changing frame count; tap photo to enlarge |
| Speaker | Fixed CC0 voice, then one-second tone | Actual clear audible output, not just a progress bar |
| Microphone | Countdown, three-second recording; separate Replay | Raw input follows speech; audible replay without squeal |
| Body touch | Tap/release then hold/release on one real sensor | Press → Release → Press → Long press → Release; no motor/light side effects |
| Lights | Side/Bottom each red/green/blue/dim/bright/breathing/off | All 14 zone/color/effect observations |
| Motion | Separate safety confirmation for each axis and STOP test | Safe open-loop movement and physically observed stop; see warning below |
| Together | Twenty-second camera/voice/alternating green zones/event run | Components coexist; no motor and no simultaneous record/play |

## Motion: no physical position feedback

Current servos provide **no physical position feedback**. The old Check position /
fresh-position prerequisite is disabled. `watcher_hw_body_request_position()`
returns `ESP_ERR_NOT_SUPPORTED`; old POSITION ABI is reserved, not a measurement.
Before every motion, clear the **whole travel**, keep manual power disconnection
available and explicitly confirm the safety page. The example first commands
**X90/Y120**, then **X93/Y120** or **X90/Y123**. With an unknown physical starting
position, the reference move itself can be large. This is not homing or calibration.
A subsequent three-degree command is still open-loop, not measured displacement.

The red **STOP** remains red while waiting; duplicate submissions are disabled.
A matching `DONE/STOPPED` is software evidence only, and still requires physical
confirmation. Natural completion is an issue for the stop test. Errors conservatively
latch body controls until reboot. Disconnect cannot deliver Stop; STM32 safety is
required. See the [current open-loop contract](servo-open-loop.md).

![Software-rendered motion confirmation showing the reference pose, full-travel warning and lack of position sensor](../../../docs/images/embedded-sdk/hardware-check-motion-safety.png)

This is a real LVGL host-rendered illustration, not a photograph of a robot. It does not establish physical display, touch or motion acceptance.

## Audio, camera and cancellation

Voice/Record/Replay open **Audio setup**. Output is 0–95, default 90, zero means
mute. Replay requests fixed **32x** boost; peak limiting may lower the actual
multiplier displayed during playback. Boost affects only the recording copy, not
microphone analog gain or fixed voice/tone; it can amplify background noise.
Raw PCM is not advertised as denoised. Stop if harsh or feeding back.
Photos/recordings are RAM-only; fixed voice is a licensed flash asset, see
[asset provenance](../examples/common/assets/README.md).

**Stop** waits for the resource owner to clean up; it is not immediate physical
stop certification. Repeated button presses cannot bypass cleanup. Failed stop/drain
disables that module; unrelated ready modules remain available. Item attempt tokens
reject stale results. MCU events are dispatched by the controller, not camera/UI callbacks.

## Optional silent serial verification

With nobody operating the screen, run one wrapper from the repository root after
closing monitor (one process owns the verified port):

```sh
bash firmware/sdk/examples/hardware_check/verify.sh --port <PORT>
```

```powershell
./firmware/sdk/examples/hardware_check/verify.ps1 --port <PORT>
```

The active IDF Python needs `pyserial`; optional `--log <LOCAL_LOG_PATH>` stores
local diagnostics. It exercises repeated photo/stream cleanup/cancel/PCM capture
and returns to the **untested** welcome page. It does not flash, scan ports, move
motors, play sounds or accept human observations. Exit codes: 0 completed, 1 timeout/
mismatched result, 2 invalid arguments. It does not establish physical hardware acceptance.

Monitor-only commands: `diag status`, `diag photo`, `diag stream`, `diag record`,
`diag cancel`, `diag reset`, `diag snapshot`. There are deliberately no serial
motor/light/speaker commands. `diag reset` does not clear a body fault latch.
A software RGB565 snapshot does not prove finger touch, sound or motion. Avoid
publishing raw diagnostic logs without checking device identifiers/private images.

## Before calling it accepted

Record public commit, SOURCE.json upstream SHA, build/toolchain, board and paired
firmware hashes, date/operator and each observed result. Issue/Skip is not Pass.
Missing image, silent audio, dropped events, failure to stop or unsafe motion is a
failed/blocked item, never a reason to tap Next. The host/LVGL regression suite is
not a robot test; [historical validation](../VALIDATION.md) is not this snapshot's
new acceptance. Official recovery follows [the public setup guide](getting-started.md),
not guessed offsets or private workspace scripts.
