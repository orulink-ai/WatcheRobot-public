# Development-preview acceptance / 开发预览验收

No stable release or fully validated firmware pairing is claimed.
本页仅记录实际执行结果；构建成功不等于整机安全验证完成。

## 2026-10-09: hardware_check v2 flashed and boot checked

- Commit preparation re-ran the scoped Windows checks: ten hardware/shared-MCU
  host projects passed 13 CTest entries per Debug/Release configuration; the
  shared example suite passed 8/8 and real-LVGL view suite passed 21/21 in each
  configuration. Scoped Python tests passed 54/54 and prototype Node tests
  passed 65/65. All four independent examples (body/head/robot/hardware_check)
  built with ESP-IDF 6.0.2. This did not run remote CI, Linux or the official
  application regression; unrelated official-application worktree changes were
  deliberately excluded from this SDK commit scope.
- The operator explicitly requested flashing this revision. Verified the selected
  ESP32-S3 port was available without terminating any process, then used the
  example's `hardware-sdk` flash entry with ESP-IDF 6.0.2. Bootloader, partition
  table and application writes all passed esptool hash verification. No other
  robot, STM32 or Himax was flashed.
- Installed application matches the preceding v2 build, SHA256:
  `826107ef1039bc2118536a0f7905e72b24d2cc56ad4fbfaf464bd9bc382ffd5a`.
  A bounded 14-second startup capture confirmed project
  `watcher_hw_hardware_check`, `Hardware check v2 | curved zones`, `start=ESP_OK`,
  LCD/touch driver creation, and `init camera=ESP_OK audio=ESP_OK`.
  The later state reported modules=15 (display/camera/audio/body available),
  body_fault=0 and the untested welcome phase. No panic or repeated reboot
  occurred in this short capture.
- Boot retained the known 32 MiB physical-flash versus 16 MiB image-header
  warning and an LEDC GPIO8 warning; no change to those configurations was made.
  Initialization/readiness does not prove visible pixels, edge-touch response,
  captured JPEGs, audible sound or mechanical behavior.
- No photo, recording, playback, light or motor test was triggered. Serial access
  was closed after the capture. Physical acceptance of the new curved controls
  and all test items remains with the operator; no commit or remote publication
  was performed.

中文摘要：新版固件已完成烧录及启动日志检查，不再处于“未刷机”状态。
启动和模块就绪正常，尚未替操作者确认屏幕效果、声音或机械动作。

## 2026-10-08: hardware_check v2 LVGL firmware (implemented and built, not flashed)

- Implemented the accepted lower-corner curved design in the standalone
  `hardware_check` example. Both 178x124 regions use the shared circle/ellipse
  predicate for drawing and native hit testing; inner arcs, circle exterior and
  the neutral gap are excluded. Labels are mirrored at (108,340)/(304,340).
  Cached scanline bounds occupy 496 bytes, without a touch-area bitmap or
  per-frame geometry allocation. The browser side-layout alternative was not
  added to the firmware.
- Preserved the 8-group/26-item workflow, fixed 32x peak-limited replay, separate
  record/replay, controller-owned body dispatch, media ownership and deliberate
  motor safety confirmation. No SDK, protocol or paired-firmware changes were
  needed for this layout. STOP stays red and disabled while waiting for the
  matching result; its pending display flag does not change motion completion
  semantics. Boot still performs no actuator tests.
- Independent review found the third Screen target overlapped the status and
  image ownership used the last rendered attempt, which could lag the controller
  after lock contention. Fixed the target top to y198 (bottom249, status starts260)
  and explicitly passed the controller-validated image attempt.
- Real LVGL 8.4.0 pointer regressions reproduced accidental navigation when a
  press was directly released outside the arc or after becoming disabled,
  without an intermediate move sample. Both failed before the input guard and
  passed afterward. The guard validates pointer release coordinates and disabled
  state; callbacks still enqueue intentions only. Keypad/encoder input is not
  filtered by pointer coordinates.
- Windows MSVC Debug and Release: real LVGL view suite **21/21** each; shared
  flow/body/audio/record/geometry suite **8/8** each. Actual rendered-pixel and
  object checks cover centering, non-overlap, report pages, pressed/disabled/STOP
  colors, enlarged images and stale-model image arrival. Prototype Node tests
  **65/65** and scoped Python integration/contract/script/failure tests **54/54**
  passed. Host snapshots are software rendering, not connected LCD observations.
  Host LVGL uses system malloc and mocked display locking; the firmware's 32 KiB
  LVGL heap, stack usage and real lock contention still require device checks.
- ESP-IDF **6.0.2**, ESP32-S3: final standard-path build passed, app size
  `0xe7d20` bytes, 77% of the 4 MiB app partition free. Bootloader, partition table
  and application are available in `hardware_check/build`; app SHA256:
  `826107ef1039bc2118536a0f7905e72b24d2cc56ad4fbfaf464bd9bc382ffd5a`.
  A separate build with a newly generated sdkconfig also passed, verifying
  Montserrat 18/20 defaults. This reused the local toolchain/dependency cache,
  not a clean-machine installation. Existing configurations without these fonts
  receive an actionable configure error; enable both via menuconfig.
- CI now includes the real-LVGL suite with immutable 8.4.0 source revision and
  Debug/Release configuration on Windows/Linux. CI was not run remotely and
  Linux was not executed locally. Independent source re-review found no
  remaining blocker/high/medium issue in the scoped changes.
- No serial connection, flashing, microphone acquisition, sound or motor/light
  operation occurred in this revision. No other robot, STM32 or Himax was
  accessed. No commit, push, PR or public-repository synchronization occurred.
  The connected robot remains on the previously installed firmware.

中文摘要：新版大弧形触控区已经移植到真实 LVGL 固件，并完成软件回归与编译；
不再只是网页稿。当前没有刷进机器人，不能把软件截图当作实机效果。刷机后先验收
显示与大区域点击，再验收拍照、喇叭、录音回放、身体触摸、分区灯光，最后在安全
空间验收运动及停止。看、听、触碰和机械安全仍由操作者确认；未确认项不得算通过。
操作步骤见 [hardware_check/README.md](hardware_check/README.md)。

## 2026-10-08: curved touch-region revision (browser only, not flashed)

- The operator rejected the large rectangular color-block design. An independent
  design review identified shape/weight, label placement, limited photo enlargement
  and inconsistent pressed/pending STOP colors. Historic design scores are not
  evidence of operator acceptance.
- Recommended lower-corner regions now use mirrored 112x100 inner curves and pale
  normal-action surfaces. Each round-screen-clipped region has approximately
  11,043 usable pixels, more than twice the former 100x52 button. Labels are mirrored
  at (108,340)/(304,340), 18px. Danger STOP remains red when pressed or pending.
  Enlarged photo is 320x180 with 10px navigation clearance and no redundant title.
- Independent screenshot review provisionally rated visual/static interaction 8.5/10,
  then suggested preserving the photo acceptance question and emphasizing pending
  status. Both were applied without adding controls. This is not user acceptance.
- New regressions failed before implementation; all 65 prototype Node tests passed
  on Windows. Browser checks across all 79 catalogue states found no content-axis
  offset or content/navigation overlap. A primary-region midpoint outside its label
  successfully opened Screen. Neutral, circular-exterior and rounded-off corner
  hit tests returned no action. Side-layout switching and hit geometry were checked.
- The design page now shows welcome, enlarged photo, active STOP and pending STOP;
  the side alternative is collapsed. Documentation was updated. No SDK, audio,
  LVGL, device, serial, GitHub or firmware-flashing operations occurred in this revision.
  Physical edge sensitivity and user acceptance of the new appearance remain open.

## 2026-10-08: large touch-region proposal and fixed replay (built, not flashed)

- The operator reported the old footer buttons were hard to tap and confirmed
  hearing their own recorded voice after manually selecting 32x on the installed
  firmware. This confirms one audible record/replay experience, not microphone
  quality across conditions or an acoustic measurement. The bounded passive
  ESP32 capture observed module mask 15, body fault 0 and an idle body-touch page;
  it did not capture that recording/replay's peak or playback parameters. No
  voice recording was exported or persisted, and no motor/light command was sent.
- Added two browser-only layout proposals: lower-corner regions (recommended,
  178x124 bounding boxes with a 56 px neutral gap) and side regions (84x154 with
  a 244 px content corridor). Each circle-clipped touch region exceeds twice
  the former 100x52 button area. Labels stay inside the circle. Browser hit tests
  confirmed blank primary-region space activates the action, the neutral gap
  and circular exterior do not, and side-layout switching works. All 79 catalogue
  states had no navigation overlap with titles, statuses, photos or result lists.
  Actual finger sensitivity near the physical screen edge remains unverified.
- TDD layout/behavior checks first failed four tests and then passed; all 61
  prototype Node tests passed. This is a deterministic HTML/CSS design proposal,
  not a bitmap used by the firmware, and no prototype page connects to hardware.
- Only `hardware_check` now requests fixed 32x replay, removes mutable boost
  state/the multiplier button, retains output volume 0–95 and shows the actual
  peak-limited multiplier during playback. The source integration guard failed
  before the change and passed afterward. Shared host coverage verifies actual
  recorded PCM is replayed at 32x for weak input without changing the original
  samples; louder/full-scale input still reduces the safe multiplier. The SDK,
  BSP microphone analog gain and existing head/robot adjustable controls are
  unchanged; fixed test voice/tone still use unity source gain. Boost is not denoising.
- Windows shared host tests passed 7/7 in Debug and Release, scoped Python
  tests passed 50, and hardware_check built with ESP-IDF 6.0.2. New unflashed app
  size `0xdd790`, SHA256:
  `a829baa3b8a176783a5c23f4ab37454ece13ed871eb52b42c5f97a411e63d642`.
  The large-region design has not been applied to LVGL. Neither this new audio
  build nor a new layout was flashed; the installed version remains the earlier
  build recorded below. No STM32/Himax writes, other-robot access, GitHub changes
  or commits occurred. CI includes the new layout tests but was not run remotely.

中文摘要：已提供左右下角／左右中部两份可预览设计稿，推荐下角方案；大区域尚未
移植到设备。用户确认旧固件 32x 时能听到自己的回放，因此体检示例改为固定 32x
目标倍率，保留防削波和总音量调节。软件测试与构建通过，当前未重刷设备。

## 2026-10-08: standalone hardware_check and silent HIL (partial acceptance)

- Added a fourth independent example, `hardware_check`, without replacing
  `body`, `head` or `robot`. Its guided English UI covers eight groups / 26 items:
  screen touch, photo/stream, speaker, recording/replay, body touch, zoned lights,
  safe small motions/stop, and combined operation. Short centered instructions
  and two footer actions separate starting a test from confirming its outcome.
  Boot is idle; Next never automatically starts the next hardware operation.
- TDD host regressions cover ordered same-sensor touch, wrong/repeated events,
  global faults, matching motion/stop sequence, stale UI intentions across the
  safety page, retained issues/skips, retries and cancellation. Shared example
  host CTest passed all seven tests in Windows Debug and Release. SDK/MCU host
  regression projects passed 13 tests in Debug. Scoped Python checks passed
  49 tests; the browser prototype's 57 Node tests passed but do not validate
  firmware rendering or physical hardware.
- All four examples built on Windows with ESP-IDF 6.0.2. The robot build exposed
  missing shared image/record sources in its CMake list; adding those sources
  restored the build. CI now includes the fourth example and the paired safe
  diagnostic scripts/tests. No fresh remote CI or Linux host run was executed
  in this session; earlier official-app results do not certify this new example.
- Verified the intended ESP32-S3 and flashed only this custom example's
  bootloader, partition table and application. All write hashes passed. Final
  application size is `0xdd810` bytes; SHA256:
  `d2f517b2c486568a05ccd8d523322c2ceaa019caee75214b1072092cef978e59`.
  This identifies a local uncommitted development build, not a published version.
  The image uses a 16 MiB layout; the device reports 32 MiB physical flash.
  No full-chip backup or official-restoration acceptance is claimed here.
- The final silent HIL runner completed with exit code 0: module mask 15 and
  body fault 0; two successive fresh JPEG captures/decodes; five-second stream
  with 11 decoded frames and `cleanup=ESP_OK`; stream cancellation after two
  frames with successful cleanup; a new photo after cancellation; and 48,000
  PCM samples (16 kHz / three seconds), `complete=1`, `ESP_OK`. In the quiet
  recording, peak was 751 and mean absolute amplitude 55. These measurements
  prove sample delivery, not speech intelligibility, audible replay or noise quality.
  PTL logged transient invalid headers/resynchronization and recovered; this is
  not a fault-injection or long-duration reliability result.
- Camera callbacks use a bounded two-frame mailbox; a separate decoder avoids
  blocking PCM delivery. Stop waits for the decoder fence before acknowledging
  cleanup. The safe serial whitelist has no speaker, light or motor commands.
  The HIL runner matched attempt/phase, accepted no human result and returned
  the device to the untested welcome page. No sound, lights or motion was
  automatically started. Serial sessions ended and released the port.
- STM32 and Himax were not rebuilt/flashed, and the other robot was not touched.
  The STM32 debug UART was quiet during bounded dual-log capture; this is not
  STM32 execution evidence. A software LVGL welcome snapshot showed centered
  content. The final layout diagnostic reported axis 206, status bottom 291,
  footer top 304 and no overlap. Neither is a photograph of the LCD or proof
  of finger interaction.
- Remaining manual gates: real screen targets and photo enlargement, voice/tone
  audibility, deliberate speech/replay without squeal, press/release/long press,
  all 14 zoned light items, mechanical limits and matched physical stop, and the
  20-second combined run. Reports stay in RAM; Issue/Skip never count as Pass.
  Motion requires explicit per-item safety confirmation and fresh position;
  link loss cannot promise delivery of Stop. No complete pairing/release claim.

中文摘要：独立全硬件测试示例已烧入明确核验的目标设备，四个示例构建和静音实机
自测通过，目前停在未验收的欢迎页。拍照、取帧、取消后复用和完整 PCM 采集有串口
证据；看、听、触碰、灯效与运动安全仍需逐项人工确认。未上传、未提交、未合并。
操作流程见 [hardware_check/README.md](hardware_check/README.md)。

## 2026-10-08: robot example flash, startup and one photo (partial HIL)

- Rebuilt the public `watcher_hw_robot` example using ESP-IDF 6.0.2. Image size
  `0xd8540` bytes; SHA256:
  `0a9cf12d5413a1f4c170023d9acd120c1020b430752153ac9df7d833b1123f9f`.
  This is uncommitted development source, not a stable release identifier.
  Public body/runtime/example and camera host checks passed again before flash.
- Verified the intended ESP32's identity and read its real partition table.
  Factory-at-`0x10000` / 4 MiB app matches the example. Saved the entire old app
  slot locally, then used IDF `app-flash`; write hash verification passed.
  Bootloader, partition table and NVS were not rewritten or erased; STM32/Himax
  and other robots were not flashed. App-slot backup is not a full-chip backup.
- Startup identifies `watcher_hw_robot` / IDF 6.0.2 / Hardware lab UX5. Body
  runtime/subscription, display, PTL camera and 16 kHz audio initialization
  returned ESP_OK. Initialization alone does not verify body readiness or sound.
- A real screen Photo click produced a fresh PTL frame: 17,046 JPEG bytes,
  SOI/EOI markers OK, `photo: ESP_OK`. The operator confirmed the normal UI and
  a new photo on screen. This validates one click/capture/display path, not
  stream restart, camera fault recovery or long-term image quality.
- No automated sound or motion test was run. Queue-loss physical stop, PTL fault
  injection, recording/playback quality and sustained combined operation remain
  unverified. Logs/backups remain local; no commit, push or GitHub operation.
- This robot UI has no screen Light/Move/Stop controls. Body lights are currently
  driven by physical body-touch events; no such events were captured in this
  session. Body touch/light acceptance remains unverified, not a hardware-failure
  diagnosis. The serial monitor was closed after this check.

中文摘要：已只更新目标 ESP32 的 robot 应用，启动和一次真实点击拍照／显示通过。
保留原启动程序、分区与设置；这不等于完成整机、故障恢复或运动安全验收。

## 2026-10-07: event-loss safety and camera-status corrections (not flashed)

- TDD: three body runtime/example targets failed the original overflow
  FAULT/NO_MEM assertion; the original camera failed the connected-device-fault
  assertion. Two new bilingual documentation checks failed before adding the
  recovery instructions. Implementations and documentation then passed.
- Body now latches event loss independently of link state. Move/light/position
  are rejected; an out-of-FIFO OVERFLOW notification precedes retained events.
  The worker attempts one stop after releasing the UART polling gate if still
  ready. Stop remains manually available on a ready transport. No physical stop
  confirmation, disconnected delivery or automatic stop retry is promised.
- Tests compile the actual body runtime and each real body/robot callback with
  hardware substitutes: lost release plus fresh position submits no movement;
  pending intent clears; retained touch order remains; lost LINK/DONE/FAULT,
  polling/dispatch notification priority, invalid output pointer, failed stop,
  UART lock boundary and disconnected first loss are covered. Drain, HELLO and
  idempotent init do not recover controls; explicit close/init clears the latch
  and old FIFO/subscriptions without motion replay.
- Camera tests cover idle READY, connected device fault, diagnostic query failure
  without populated output, disconnect precedence, recovery and existing
  lifecycle/ownership checks. Healthy diagnostics do not clear lifecycle failures.
- Windows verification: public tools pytest **107 passed**, canonical related
  pytest **37 passed**. All 23 host projects passed Debug/Release (46 suite/config
  runs), with assertions active; the changed body/camera suites were rerun after
  the final edits. Both repositories' body/head/robot built on ESP-IDF **6.0.2**;
  the isolated official S3 regression also passed. Thirty-nine SDK files and both
  body/robot callbacks match snapshots after newline/trailing-end normalization.
  Changed C/headers passed clang-format 18.1.8; ten public diagrams parsed.
- Recovery contracts, migration notes, bilingual experiments and the audit were
  updated. H1/M1 are software-corrected; two Low follow-ups remain deferred.
  No flash, reset, serial access, sound/motor command, STM32/Himax write,
  credential access, commit/push or GitHub operation occurred. No fresh Linux,
  remote CI, clean-machine install or hardware fault injection is claimed.

中文摘要：已修复丢事件后仍可提交控制和连通摄像头漏报故障的问题，先复现再修复，
软件测试与构建通过。真实队列饱和下的机械停止、PTL 故障恢复和发布门禁仍待实机验收。

## 2026-10-07: namespace and consistency audit (local source, not flashed)

- Unified hardware component/header/API/type/constant names and the three
  example app targets under `watcher_hw_*` / `WATCHER_HW_*`. Task/host names,
  CI paths and active bilingual docs agree. This is a preview source-API break;
  [MIGRATION.md](MIGRATION.md) explains consumer updates and fresh build directories.
  Earlier entries retain the actual names/checksums of the tested old firmware.
- TDD namespace guards failed against the old source before the rename.
  Additional script regressions exposed blank-port and explicit-empty-argument
  differences. Both platform entries now reject them before IDF, and mock-IDF
  checks confirm argument/default/exit-code parity without real flashing.
  Documentation regressions first failed 10 cases, then passed after correcting
  preview availability, page navigation and project-derived artifact filenames.
- Final Windows software checks: public tools pytest **105 passed**; canonical
  SDK/script/audio-output pytest **37 passed**. Twenty-three host projects passed
  both Debug and Release (46 suite/config runs). Canonical and public body/head/
  robot builds all passed ESP-IDF 6.0.2, as did the isolated official S3 regression.
- Actual example dependency graphs contain only their selected hardware modules:
  body has core/body, head has core/audio/camera/display, robot has all five.
  No inspected graph pulled watcher_sdk, voice_service, behavior_state_service,
  ws_client or hal_display. Seventy-three scoped source/config/asset/script files
  match between canonical/public copies after newline/trailing-end normalization.
  Nine Mermaid diagrams parsed; renamed first-party C/headers passed the
  canonical clang-format 18.1.8 check.
- The audit also found unresolved non-naming risks; see [AUDIT.md](AUDIT.md).
  An isolated Debug/Release characterization compiled the real event FIFO and
  the exact current body-example callback, replacing every hardware API with
  a mock. Normal case: dropped=0, requests=1, stops=1, moves=0. Queue saturation:
  dropped=1, requests=1, stops=0, moves=1. Lost release can permit a later motion
  submission. This exposes a safety defect; it is not successful safety acceptance
  or physical motion evidence. SDK camera status also omits a connected peer's
  device-fault status. These runtime behaviors were not fixed by renaming.
- No robot reset, flash, serial read, sound, motor operation, STM32/Himax write,
  GitHub operation or credential access occurred. No new commit or push was made.
  No fresh Linux execution, remote CI, clean-machine install or renamed-source
  hardware acceptance is claimed. Existing stable-release gates remain open.

中文摘要：本轮完成全链路改名、双平台参数语义和中英文文档修正，并通过软件回归。
同时用硬件替身复现了“丢失松手事件后仍可能提交运动”的风险，另确认摄像头就绪状态
可能遗漏设备故障；这些需要单独修复，不能因改名、构建通过而宣布已解决。

## 2026-10-07: UX5 recording/playback observation and local commit checks

- One deliberate three-second recording produced 48000 samples, peak 4338
  (-18 dBFS) and mean absolute value 346. Four captured playback operations
  returned ESP_OK with Output 90, requested boost 32x, effective boost 4x and
  clipped=0. The operator reported that the previously perceived squeal did
  not seem present in this trial. Raw PCM was not exported or spectrally
  analyzed; the cause of the earlier intermittent noise remains unconfirmed.
- Zero digital clipping is not proof of clean microphone input or freedom
  from analog interference/distortion. If 1x makes speech inaudible, not hearing
  noise at 1x does not establish its absence. Test at a comfortable audible
  level and retain raw input separately from boosted playback when diagnosing.
  No live software microphone-to-speaker loop was found in the reviewed
  example. That does not exclude recorded noise or board-level coupling.
- This follow-up only passively captured the existing firmware's serial output;
  no device reset, automatic sound, motion command or new flash was performed.
  Test-sample hearing, physical mute/volume controls, recording quality and
  sustained combined operation still need dedicated acceptance.
- Local closeout checks: public tools pytest 77 passed; canonical SDK/script/
  audio-output pytest 19 passed. Shared example state/audio/player/asset and
  SDK audio lifecycle CTest passed Debug/Release on Windows. Public body/head/
  robot, canonical head/robot and isolated official S3 builds passed IDF 6.0.2.
  No fresh remote CI, Linux execution or clean-machine setup is claimed.
- Nine Mermaid workflow/architecture diagrams passed parsing with Mermaid
  11.12.2; their text instructions remain usable without diagram support.
  Twenty-eight scoped source/design files match the canonical/public copies
  after line-ending/trailing-whitespace normalization. Changed first-party
  C/header format checks passed with clang-format 18.

中文摘要：本轮完成一次 3 秒录音和日志中四次回放，实际增强为 4x，数字削波为 0；
用户本轮似乎未听到尖叫，但没有导出原始录音，不能宣布噪声原因已确定或音质已验收。
本轮仅被动读日志；新增文档流程图、架构图及导航测试不替代硬件发布门禁。

## 2026-10-07: UX5 speaker-test follow-up

This is still a development preview, not full head/body acceptance.

- A same-source bench comparison replayed one captured JoyInside reply through
  official firmware and SDK. The operator confirmed approximately equal loudness.
  Source: mono PCM16 at 16 kHz, 44358 samples, peak 32301, RMS 5891.
  Official output 100 and SDK output 95 both read DAC volume register `c1`
  (BSP cap 95), mute register `00`, PA output latch 1/output direction.
  Source/DMA peak and nonzero counts agreed. This is not measured SPL, nor proof
  of waveform equality; the original 660 Hz tone remains a separate question.
- UX5 now offers `Test speaker` -> `Test voice` / `Test tone`. The voice is a
  bundled synthetic CC0 formant sample with recorded provenance, not the private
  cloud response. Both tests show Output, progress and finished/error feedback.
  Replay boost remains recording-only; no SDK automatic gain was introduced.
- Playback copies immutable sources into scratch and waits for
  `watche_hw_audio_drain(1000)` before releasing busy. A failed write's possibly
  submitted prefix is drained, never automatically replayed. Drain failure
  cannot report successful completion. The lease is retained for later operations.
- Output 0 now sets codec mute, rather than merely reducing its gain.
  Nonzero output unmutes. Failure propagation and 0 -> 95 transitions are covered
  by tests extracting the actual HAL function, with assertions enabled in both
  Debug and Release.
- Windows MSVC shared state/audio/player/asset CTest passed Debug and Release in
  canonical and public copies; SDK audio lifecycle CTest passed both configurations.
  Public tools: 75 passed. Canonical SDK/script/output checks: 19 passed.
  Canonical/public head and robot, public body, and official S3 built on IDF 6.0.2.
  The official build used an isolated build/sdkconfig directory after its existing
  cached BLE profile was rejected; the old configuration was not overwritten.
  This is a fresh build-directory check, not a clean-checkout/install claim.
  Linux execution and fresh
  remote CI were not performed locally.
- Public head `0xd34d0` bytes was fully flashed with its own example entry point
  after verifying the compatible factory-at-`0x10000` / 4 MiB app layout. Write
  hash verification passed. Final head SHA256:
  `695cb07ee55a5bba7259c4183487a4933bc08930089fcdb97afddb1fd1b2f5c5`.
  Startup identifies UX5; display, PTL camera and audio initialization returned
  ESP_OK. No automatic sound, body command, STM32/Himax update or NVS erase.
- New public-sample hearing, physical settings/mute, microphone/replay quality,
  camera UI rendering and combined robot acceptance remain explicit manual gates.
  API delivery alone cannot establish audible/visible/mechanical success.

Private credentials, cloud audio, NVS backups and raw device logs are not bundled.


2026-10-07 UX4: manual tone peak 24000 (previous UX3 12000), default output 90
(previous 80), BSP cap unchanged at 95. Replay boost adds opt-in 16x/32x and is
explicitly Play-only. Default 1x, raw PCM immutability, full-record peak limiting,
scratch saturation and tone fades retained. No SDK/HAL or analog gain changes.
Calculated source+codec change at defaults is about +11 dB, not measured acoustic
loudness. Noise and distortion require manual hearing checks, still pending.
Canonical/public CTest passed Debug/Release; public pytest 73 passed; internal
SDK/output pytest 18 passed; public head/robot built on Windows IDF 6.0.2.
Device identity and 0x10000/4MB app layout verified for head-only app flashing.
App-flash hash passed; startup identified UX4 and display/camera/audio ESP_OK.
Public head SHA256:
`ef804bcd443376bec0c8dc4d7f98349340f9b253f0b95f3ea04f4054098d4f37`.

2026-10-07 UX3 audio follow-up: the canonical shared example now exposes output
volume 0–95 (default 80), explicit replay boost 1x/2x/4x/8x and a digital peak
dBFS mic meter. Boost changes only writable playback scratch and is limited
against the whole recording peak; raw saved PCM and analog mic gain are unchanged.
Volume setters run only on the media owner, outside the LVGL lock; applied UI
values update on setter success. Manual 660 Hz test tone now peaks at 12000.

Public head/robot built with Windows IDF 6.0.2. Public tools pytest passed 72;
internal SDK/output tests passed 18; shared state/audio host CTest passed Debug
and Release. App-only flash hash passed and boot identifies UX3 / watche_hw_head,
with display/camera/audio ESP_OK. No body commands or STM32/Himax flashing.
The previous manual 24 kHz HAL/SDK comparison was audible but quiet; UX3 16 kHz
tone, adjusted volume, deliberate recorded speech and replay boost still need
operator hearing confirmation. No complete audio or integrated acceptance claimed.
Public head binary SHA256:
`0dacd67e9f18ab0b7afa45e0036206eca2156ea8b1ad118ed85ad03dd7c7fd99`.

| Item / 项目 | Status / 状态 |
| --- | --- |
| Public body/head/robot builds | Windows IDF 6.0.2 passed / Windows 构建通过 |
| Separate developer project | Complete standalone flash and body command-chain HIL passed; details below / 独立完整烧录及身体命令链路测试通过，详见下文 |
| Windows/Linux remote CI | All jobs passed at `4d00c5d`; final review corrections require fresh CI / 保活修复全部 CI 通过，合并前审查补充改动需重新验证 |
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
