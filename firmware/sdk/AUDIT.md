# SDK consistency audit / SDK 一致性排查（2026-10-07）

本轮区分“已修正的一致性与运行问题”和“仍待验证／后续处理的事项”。
H1 丢事件与 M1 摄像头状态已完成独立修复和软件回归；不代表完成新的实机验收。

## Scope / 范围

Five hardware modules, public headers, example projects/outputs, CMake and CI,
Windows/Shell entry points, bilingual guides, and nearby lifecycle/status paths.
Canonical ESP32 source and the public delivery snapshot were updated together.
Unrelated user changes, board branding, wire protocols and the service-dependent
`watcher_sdk` were not renamed. No hardware, GitHub or credential operations.

## Corrected / 本轮已修正

- Public components/headers/APIs/types/constants now use `watcher_hw_*` /
  `WATCHER_HW_*`; matching host targets, internal task names and CI paths agree.
  Applications must follow [MIGRATION.md](MIGRATION.md). Old evidence stays historical.
- Shell no longer treats explicit empty example/action arguments as defaults.
  Both entries reject blank flash/monitor ports before IDF (exit 2); omitted
  arguments retain robot/build defaults, and valid calls preserve IDF arguments
  and exit code. Tests use a mock IDF, never a real flash.
- English/Chinese preview status and custom flashing entry descriptions agree.
  Audio experiments include the return from Speaker test to the main page before
  Record/Play. Quickstarts identify each project's generated app/ELF directory,
  bootloader/partition needs and IDF-managed flashing arguments.
- Namespace contracts scan source filenames, contents, active docs and CI.
  Only migration notes and historical validation entries may retain old names.

### H1 corrected: Lost release / 溢出丢失松手事件（原 High Risk）

**Location:** [body runtime](components/sdk/watcher_hw_body/watcher_hw_body.c),
[public contract](components/sdk/watcher_hw_body/include/watcher_hw_body.h),
[body example](./body/main/app_main.c) and robot's matching callback.

The earlier characterization reproduced a lost release followed by a motion
submission (dropped=1, moves=1), not physical robot movement. The original source
also failed the new FAULT/NO_MEM regressions before implementation.

The runtime now latches event loss separately from transport readiness, rejects
move/light/position requests, and delivers OVERFLOW ahead of retained FIFO events
without competing for a queue slot. The payload reports cumulative dropped and
the first best-effort stop result. That stop runs after unlocking the UART gate;
manual stop remains available on a ready transport, not after disconnection.
Touch FIFO order and observable drops remain intact; no latest-state replacement.
Body/robot cancel pending intent. Draining, idempotent init and HELLO cannot clear
the latch; explicit close/init outside dispatch, resubscription and new intent
are required. There is no automatic retry, recovery or motion replay.

**Verification:** real runtime plus each real example callback, with hardware
mocked: lost release/fresh position, lost LINK/DONE/FAULT, priority notifications
in polling and dispatch, stop success/failure and UART gate boundary, disconnected
first loss, reconnect/drain/init still locked, close/init resets and no replay.
Windows Debug/Release pass. Hardware saturation/stop remains unverified.

### M1 corrected: Connected camera fault / 连通不等于就绪（原 Medium）

**Location:** [SDK status](components/sdk/watcher_hw_camera/watcher_hw_camera.c),
[PTL diagnostics](components/hal/hal_camera/src/hal_camera_ptl.c#L638),
[test diagnostics](components/sdk/watcher_hw_camera/test_support/host/include/hal_camera.h).

The original connected-fault regression failed. Status now prioritizes diagnostic
query failure, disconnection, then device status; a connected fault is not READY.
Idle remains valid; healthy diagnostics clear transient errors, not lifecycle
failures or external ownership. Tests represent the real HAL's status field.
**Verification:** connected fault, query failure without populated output,
disconnect precedence, idle readiness, recovery, lifecycle failure and existing
ownership tests pass Windows Debug/Release. Real PTL fault injection is pending.

## Blockers

None found in the inspected patch. This is not approval of a stable release,
mechanical safety, audio quality or combined hardware acceptance.

## High Risk

None found remaining in the two corrected paths under the tested contracts.
H1 history and current limitations are recorded above.

## Medium/Low

### L1: Implementation dependencies leak into public headers / 依赖边界（Low）

**Location:** [audio CMake](components/sdk/watcher_hw_audio/CMakeLists.txt#L1)
line 1; [camera CMake](components/sdk/watcher_hw_camera/CMakeLists.txt#L4)
line 4.

HAL dependencies are public even though these SDK headers only require core.
This weakens encapsulation; it is not evidence of an existing resource bypass.
**Recommendation:** move these HAL dependencies to PRIV_REQUIRES in a separate
change. Keep dependencies that provide genuinely public body/display types.
**Verification:** a single-module consumer and actual transitive include graph.
Not changed here.

### L2: Example startup failure retains resources / 示例失败重试边界（Low）

**Location:** [head_demo_start](./common/head_demo.c#L673)
lines 673–692; [example header](./common/head_demo.h#L3)
lines 3–5.

After media-task creation fails, UI objects remain but actions becomes NULL,
so an extra caller can retry and create duplicate UI. Earlier queue allocation
failure retains an initialized display. Current head/robot call start only once;
the header promises no dynamic shutdown or retry loop. This is not a claim of
repeated leakage on the normal example path.
**Recommendation:** explicitly make one startup attempt process-lifetime, or
implement ownership-aware rollback if retries are later supported.
**Verification:** queue/lock/task creation failure and repeated manual calls.
Not changed here.

## Missing Verification

- No fresh Linux execution, remote CI, clean-machine setup or renamed-firmware HIL.
- Full STM32/Himax pairing identifiers and all stable-release gates remain open.
- New regressions cover event-loss locking and camera status with hardware
  substitutes. They do not establish physical stop or PTL fault recovery.
- The deferred startup retry/rollback scenario still lacks a dedicated test.
- [VALIDATION.md](VALIDATION.md) separates software evidence from physical results.

## No Finding Areas

The inspected rename preserves MCU packet formats, angle/duration ranges,
event payload layouts, buffer ownership comments, audio format/volume/drain
contracts and optional-module entry points. No new official-app dependency,
resource manager, duplicate SDK implementation or automatic startup motion
was introduced. The new overflow path attempts a stop only on a ready link;
host checks do not establish that physical stopping completed.
