English | [简体中文](../../../products/watcherobot/capabilities.md)

# Robot capabilities

These are device primitives available to a managed Application. Complete [SDK setup and pairing](../../sdk/index.md), then run with `watcherobot app run`.

Checked on **2026-10-15** against Python SDK **0.1.12** (`cc0a319`) and ESP32 source `7699527`. The public repository's pinned `python-sdk/` may be older; check the installed SDK and device capabilities first. These source revisions are not a claim of a released compatible firmware bundle. Availability depends on your SDK, firmware, hardware, and installed resources; not every device supports all entries.

## Motion {#motion}

| No. | Capability | API | Notes |
| --- | --- | --- | --- |
| 01 | Move to an angle | `robot.motion.move_to()` | Pan, tilt, duration; returns a Job. Protocol range 0–180 degrees, subject to mechanical limits. |
| 02 | Continuous targets | `robot.motion.set_target()` | Either axis; no Job. Acceptance does not mean arrival. |
| 03 | Preset action | `robot.motion.play_action()` | Installed device action ID, not a host path. |

Requires `motion`. Finish with `robot.motion.stop()`. Do not compete with face tracking for the gimbal. Clear the surrounding space. The baseline does not guarantee different actuator curves for `linear` and `ease_in_out`.

## Lights and display {#display}

| No. | Capability | API | Notes |
| --- | --- | --- | --- |
| 04 | Color and brightness | `robot.lights.set_color()` | `light`; brightness 0–1; zones `side`, `bottom`, `all`. |
| 05 | Light effects | `robot.lights.play_effect()` | `light`; blink, breathing, rainbow, status pulse. `repeat=0` loops. |
| 06 | Animation | `robot.animation.available_ids`, `robot.animation.play()` | `animation`; choose announced resources and stop loops explicitly. |
| 07 | Code expressions | `robot.expression_runtime.start()` / `update()` / `stop()` | Eyes, gaze, color, blinking, preset/pixel/vector accessories. |

End lights with `robot.lights.off()`. Firmware owns privacy indicators. Prefetch requires `animation.prefetch.v1` and does not start playback. Custom accessories require `expression.runtime.v3` plus `expression.pixel_accessory.v1` or `expression.vector_accessory.v1`. Pixel masks are 51×51 encoded as 652 hexadecimal characters; vectors use a 412×412 canvas.

## Sound {#audio}

| No. | Capability | API | Notes |
| --- | --- | --- | --- |
| 08 | Device sound | `robot.audio.play()` | `audio`; an installed SD sound ID. |
| 09 | Host file / PCM | `robot.audio.play_file()` / `play_pcm()` | `audio.stream`; PCM is 24 kHz, mono, 16-bit S16LE, up to 4 MiB. |
| 10 | Microphone | `robot.microphone.open()` / `open_pcm()` / `record_pcm()` | `microphone`; Opus packets, PCM frames, timed recording. PCM output: 16 kHz, mono, 16-bit. |

Ordinary playback and microphone capture are mutually exclusive. `play_pcm()` submits a complete buffer, not continuous writes. Opus packets are not PCM WAV; decoding needs host dependencies. Use `record_pcm()`, not the baseline's unsupported `record()`.

## Vision {#vision}

| No. | Capability | Capability identifiers | Notes |
| --- | --- | --- | --- |
| 11 | Photo capture | `camera.capture`, feedback additionally `camera.capture.feedback.v1` | `robot.camera.capture()` / `capture_with_feedback()` return JPEG. Feedback also uses display and sound; conflicts do not fall back to ordinary capture. |
| 12 | Device inference | `vision.inference.v1`, preview additionally `vision.inference.preview.v1` | Query/select models, run inference, retrieve results and images. |

Query installed models with `robot.vision.models()` and start with `robot.vision.start_inference(model_id, preview=True)`. Availability depends on paired Himax firmware and ESP32 build options; `verified` is not an accuracy guarantee. Stop existing tracking, inference, recording, or RTC video before switching camera use.

## Touch and roller {#inputs}

| No. | Capability | Capability identifier | Event source |
| --- | --- | --- | --- |
| 13 | Back touch | `input.back_touch` | `back_touch`: action and sensor ID. |
| 14 | Screen touch | `input.screen_touch` | `screen_touch`: coordinates. |
| 15 | Roller | `input.roller` | `roller`: signed step delta, not absolute angle. |

Use `robot.inputs.wait(timeout=...)` and filter `event.source`. Inputs share a queue. `clear()` discards queued events; `dropped_events` counts queue drops.

## Composed capabilities {#composed}

These combine resources; do not assume unrestricted parallel operation.

| No. | Capability | API / identifiers | Boundaries |
| --- | --- | --- | --- |
| 16 | Face tracking | `robot.face_tracking.start()` / `stop()`; `face_tracking.control.v1` | Camera and gimbal. Stop with hold or recenter policy. |
| 17 | Tracking preview | `robot.face_tracking.open_preview()`; `face_tracking.preview.v1` | Images, face boxes, telemetry; context exit closes preview and stops tracking. |
| 18 | Recording | `robot.recordings.start()` / `start_host()` | `recording.device.v1` / `recording.host.v1`; check `recording.device.av.v1` for AV, `recording.screen_preview.v1` for screen preview. |
| 19 | RTC | `app.rtc.start()` / `stop()` | Video: `rtc.video.mjpeg.v1`; duplex audio: `rtc.audio.full_duplex.v1`; AV: both. |
| 20 | Behaviors, works, clips | `robot.behavior.play()`, `robot.works.play()` / `play_expression()` | Check `behavior`, `resource.work.play`, `resource.work.expression.play`; installed IDs only. |

Recording downloads contain manifests and WREC segments; MP4/WAV conversion runs on the host. Host recording `close()` only closes local reception: stop the device, consume the final frame, then close when ending early. Long host recordings require heartbeats every 5 seconds; unlimited device recordings also require heartbeats. Requested FPS is not guaranteed FPS.

RTC `start()` controls a session; a real media client must exchange SDP/ICE. Success does not prove a connected call. `reset()` clears local state and cannot replace `stop()`.

Works return acknowledgments, not Jobs. ASR, LLM, TTS, and AI conversation are Application-level model/service compositions, not hardware primitives.

## Inspect capabilities in an Application {#inspect}

This example queries only and does not actuate hardware. Use it as `app.py` in an initialized Application. Run `watcherobot app run`, not `python app.py`.

```python
import asyncio
from watcherobot.application import ApplicationContext


def inspect(app):
    robot = app.robot
    print(robot.capabilities)
    print("Motion supported:", robot.supports("motion"))
    print("Camera supported:", robot.supports("camera.capture"))


async def main():
    async with ApplicationContext.from_environment() as app:
        await asyncio.to_thread(inspect, app)


asyncio.run(main())
```

Run synchronous device calls in a worker thread. ACK means acceptance; Jobs require terminal-state checks. `Job.wait()` timeouts do not stop hardware; cancellation requests do not prove it stopped. Release sessions with their respective `stop()` / `close()` methods.

This catalog reflects documentation and static checks, not comprehensive physical validation. See [troubleshooting](../../troubleshooting/index.md) or the [community](community.md).
