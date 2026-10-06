# Development-preview acceptance / 开发预览验收

No stable release or fully validated firmware pairing is claimed.
本页仅记录实际执行结果；构建成功不等于整机安全验证完成。

| Item / 项目 | Status / 状态 |
| --- | --- |
| Public body/head/robot builds | Windows IDF 6.0.2 passed / Windows 构建通过 |
| Separate developer project | Built and app-flashed; partial HIL passed / 独立构建与应用烧录通过，部分实机通过 |
| Windows/Linux remote CI | All preview jobs passed at `e2aa951` / 音频及显示修复版本全部通过 |
| STM32 commit/version | Observed `25d70f5a78c1`, clean, FW 0.1.0, HW 1; capability bitmap `0x45` (no LED) / 当前配对不能通过完整身体能力门禁 |
| Himax PTL SHA256 | Not recorded / 未记录 |
| Hardware: motion, lights, body touch | Not accepted; body rejects missing LED capability / 未验收，缺少灯光能力时明确拒绝就绪 |
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
