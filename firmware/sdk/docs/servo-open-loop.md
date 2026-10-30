English | [简体中文](servo-open-loop_zh.md)

# Public hardware contract: open-loop servos

Current supported servos have **no physical position feedback**. This describes
hardware behavior, not a claim that this public snapshot has passed bench acceptance.

## Commands are not measurements

- `watcher_hw_body_request_position()` and non-null position subscriptions return
  `ESP_ERR_NOT_SUPPORTED`; unregistering a null callback remains idempotent.
- Historical POSITION frames and reserved protocol identifiers are not current
  measurements. Do not use them to gate motion, infer pose or recover limits.
- PWM, command limits, explicit lock/release, STOP, ACK/NACK and DONE/FAULT remain.
  ACK/DONE, requested coordinates and PWM-derived angles cannot establish actual
  position, absence of a stall or physical stopping. STOP is not an emergency stop.

## Clear the entire movement envelope

Before **every** motion, provide human clearance of the whole possible travel,
keep operator-controlled power disconnection available and verify paired STM32
firmware compatibility. The check first requests **X90/Y120**, then X93/Y120 or
X90/Y123. The starting position is unknown: the reference move may be large.
This is not homing, calibration or a measured three-degree displacement. The
example does not automatically return to center on exit.

A disconnected ESP32 cannot deliver STOP. Protocol completion, reconnect and FIFO
drain do not establish mechanical safety; fault recovery requires explicit
reinitialization and new user intent, never automatic motion replay.

## Acceptance boundary

Host tests establish software state/sequence behavior only. A human or an
independent future fixture must observe motion direction, full limits and actual
stop. Record paired firmware versions, public source/build hashes and each result.
Historical feedback calibration is not a current prerequisite and cannot replace
physical evidence. This SDK does not update STM32/Himax firmware. A pair lacking
verified compatibility or complete official recovery assets is a **do-not-flash** blocker.

See [safe setup and recovery](getting-started.md), [26-item check](hardware-check.md)
and [current validation facts](../SYNC_VALIDATION.md).
