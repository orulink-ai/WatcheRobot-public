# Development-preview acceptance / 开发预览验收

No stable release or fully validated firmware pairing is claimed.
本页仅记录实际执行结果；构建成功不等于整机安全验证完成。

| Item / 项目 | Status / 状态 |
| --- | --- |
| Public body/head/robot builds | Windows IDF 6.0.2 passed / Windows 构建通过 |
| Separate developer project | Complete standalone flash and body command-chain HIL passed; details below / 独立完整烧录及身体命令链路测试通过，详见下文 |
| Windows/Linux remote CI | All jobs passed at `4d00c5d`; final integrated commit checks are recorded in [PR #6](https://github.com/orulink-ai/WatcheRobot-public/pull/6) / 最终整合版本 CI 以该 PR 的提交检查为准 |
| STM32 commit/version | Local patched `25d70f5a78c1`, dirty, FW 0.1.0, HW 1, capability `0x47`; original image was `0x45` / 当前为本地灯光声明补丁版本 |
| Himax PTL SHA256 | Not recorded / 未记录 |
| Hardware: motion, lights, body touch | Repeated motion/light/stop command chain passed on patched pairing; body touch and full acceptance pending / 补丁配对下重复运动、灯光、停止链路通过，身体触摸及完整验收待完成 |
| Hardware: JPEG, PCM, display | Partial results below; not full acceptance / 部分结果见下文，非完整验收 |

2026-10-06: ten Windows MSVC host projects passed in Debug and Release
(22 CTest executions, including UART concurrency tests). Public tool and SDK
contract pytest suite passed (63 tests after the display regression test, including Windows/Shell script parity). Builds
resolved dependencies in new project directories using the installed IDF and
local download cache; this is not a clean-machine installation claim.

2026-10-06 hardware smoke: a separate ESP-IDF project with its own `app_main`,
CMake and partition table consumed a GitHub clone of the public preview SDK,
with the audio/display fixes applied locally. Only the ESP32 application was
updated during these retests; STM32 and Himax were not flashed. No motor commands
were sent. Observed on ESP32-S3 rev 0.2, 8 MB PSRAM:

- LCD and SPD2010 touch driver initialization returned `ESP_OK`; physical screen
  rendering was confirmed by the operator; finger interaction still needs confirmation. Fix: prepare the
  shared IO expander and enable the LCD power rail before panel initialization.
- PTL single capture returned `ESP_OK`, JPEG 16,324 bytes with SOI/EOI markers.
  Full JPEG decoding, continuous streaming and restart remain unverified.
- PCM 16 kHz capture/play/close and two 24 kHz init/capture/play/close cycles
  returned `ESP_OK` without I2S errors. BSP-owned format configuration preserves
  its physical microphone slot and codec-open bookkeeping. These return values
  alone do not verify audible output, sample quality or long-term leak freedom.
- Body reached `ESP_ERR_NOT_SUPPORTED`: peer advertises motion, touch and power,
  but no LED capability. This failure did not prevent head modules from working.
  Do not bypass the gate or list this firmware as a fully compatible pairing.

The local pre-test backup covers the lower 16 MB only; boot logs report a 32 MB
physical flash. No full-chip backup or official-restoration acceptance is claimed.
Backups may contain device secrets and must never be published.

Release gates: clean-checkout setup and all builds; ordered events and observable
overflow; motion limits/stop/rejection/completion/fault; no reconnect replay; JPEG
decode and stream restart; PCM 16k/24k and volume; screen touch; concurrent lifecycle;
missing/incompatible hardware isolation; custom flashing and official restoration.
Record board revision, SDK commit, STM32 version, Himax checksum and evidence.

2026-10-06 follow-up (not full body acceptance): backed up the original STM32
128 KiB locally, then flashed the original `25d70f5a78c1` source with only
the three-line `APP_LED_ENABLE` capability-advertisement correction. OpenOCD
reported `Verified OK`. Test-image SHA256:
`ac385f806bf634b714c99c5219129f5141f469f2d0ffcd8494aedd7e4af217a0`.
This is a local patched baseline, not an unmodified released firmware pairing.
The same correction passed all five host suites on the current STM32 checkout.

The separate developer application was rebuilt with IDF 6.0.2 and app-flashed
successfully. It offers screen buttons for color changes, a manually initiated
fixed-target motion sequence (90/120 degrees over 3 s, then X=95/85/90 degrees
with Y=120), and stop. There are no explicit position queries; fixed timings
are not evidence of motion completion. The initial move may be larger than
5 degrees because the starting position is unknown. No automatic motor test
runs at boot, and no reconnect replay is implemented in the test application.
Physical lights, button touch, motion and stop still await operator confirmation.
Per the operator's request, no runtime feedback was read during this follow-up.

2026-10-07 runtime regression and automatic HIL:

- The operator confirmed one successful screen-driven color change and motion,
  then reported `Body not ready`. ESP32 logs confirmed later clicks reached the
  button handler but light submissions returned `ESP_ERR_INVALID_STATE`.
- The STM32 baseline disables periodic sensor reports. The SDK incorrectly
  treated five seconds of quiet as disconnection. A diagnostic UART reader can
  also cause a transient gate-contention error that latched the SDK in FAULT
  while the lower link stayed READY. Host regressions reproduced that failure
  before the correction; Debug and Release passed afterward, along with 15
  public SDK contract/script tests.
- The worker now takes the recursive UART gate before polling, skips transient
  contention, marks recoverable transport failures degraded for re-handshake,
  and probes quiet peers with HELLO without clearing READY. Missing responses
  still invalidate readiness; recovery does not resend movement. No position
  or sensor query is needed for liveness.
- A device carrying a different partition table failed to boot after an
  app-only update (`0x20000` device app versus `0x10000` example app). Complete
  example bootloader/partition/app flashing restored startup. Application-only
  flashing requires checking the connected device's layout.
- Two 55-second captures used ESP32 and the positively identified STM32 debug
  UART. In the final run, three light submissions each completed, fixed-target
  motion received ACK and successful completion, stopping the in-flight motion
  emitted `STOPPED` for its original sequence after 991 ms, and a second motion
  sequence completed. Every periodic health observation remained READY; no
  timeout or invalid-state rejection occurred. STM32 `servo_apply` logs confirm
  execution of the motion path. These are command/execution results, not ADC
  position verification. The first capture's stop-completion check lacked the
  needed application event log; the second captured it and all nine checks
  passed. This is not a long-duration or mechanical-limits acceptance claim.

Local independent test-app SHA256:
`95eec7390ca4d1d5afccb2c7d3eeab30f006fcedd8a7b11030a7c9d832e27451`.
The local test app adds serial commands and periodic health logs; these test
hooks are not part of the SDK API or the three public example entry points.

2026-10-07 pre-merge review corrections:

- SYS ACK/NACK now use the existing motion service's last-32-command sequence
  window. HELLO/LED replies no longer masquerade as motion events. Older replies
  outside this bounded window are ignored; typed DONE/FAULT remain unfiltered,
  preserving the original motion sequence when STOP completes an earlier move.
- Body lifecycle changes during event dispatch, and concurrent closes, are
  rejected with `ESP_ERR_INVALID_STATE`. A timed-out close remains retryable.
  The new host regressions failed before each fix and passed afterward.
- These final corrections have host/build coverage; the prior HIL application
  checksum refers to the earlier runtime correction, not this final source.

The ESP32 development branch advanced during delivery. Its display-owned DMA
staging improvements were retained during integration, and the public snapshot
uses the same display/BSP source. LCD release host tests now exercise SPI drain
and ISR-detach failures, ensure resources remain intact and the LVGL lock is
released, then retry successfully. Both LCD staging/release tests passed on
Windows; public CI also runs them on Windows/Linux in Debug/Release.
