English | [简体中文](../../guide/assembly.md)

# Whole-robot assembly tutorial

Use this page with the whole-robot BOM, the visual parts clips, and the complete video to assemble the WatcheRobot structure, electronics, wiring, and fasteners. Complete [PCB fabrication and SMT assembly](../../../../docs/pcb-fabrication.md) first and download the [whole-robot assembly BOM](../../../../hardware/pcb/bom/BOM_Assembly_整机组装料.xlsx). Quantities, cable variants, and fastener specifications come from the BOM rather than the video alone.

::: warning Safety
Keep power disconnected while assembling, inserting flat cables, or changing wiring. Do not force a servo, infer connectors from wire colors alone, or proceed when battery polarity, flat-cable orientation, or screw length is uncertain.
:::

## 1. Identify the main parts {#parts}

### Structure and major modules

<video controls muted loop playsinline preload="metadata" style="width: 100%; border-radius: 12px;">
  <source src="/media/docs/assembly/parts-overview.mp4" type="video/mp4">
</video>

| Part | Purpose and check |
| --- | --- |
| Watcher head | Provides the display, camera, microphone, and speaker; confirm compatibility with the body and connectors. |
| Body, rear cover, center axis, base, and bottom cover | Form the enclosure and motion structure; remove burrs and test-fit holes first. |
| Bearing | Supports center-axis rotation; verify orientation and smooth movement. |
| Non-slip pad | Attaches to the bottom after final inspection. |
| STM32 body control board | Connects the servos, head, and body electronics. |
| Side-light PCB | Mounts inside the body for status lighting. |

### Electronics and wiring

<video controls muted loop playsinline preload="metadata" style="width: 100%; border-radius: 12px;">
  <source src="/media/docs/assembly/electronics-overview.mp4" type="video/mp4">
</video>

The set includes the battery, light component, debug harness, FPC flat cable, bottom power PCB, Type-C harness, touch copper foil, two feedback servos, and power cable. Confirm the exact name and location of the small machined part from the BOM and the physical set.

### Screws and fasteners

<video controls muted loop playsinline preload="metadata" style="width: 100%; border-radius: 12px;">
  <source src="/media/docs/assembly/fasteners-overview.mp4" type="video/mp4">
</video>

The source shows M2×6, M2×8, and M2×20 screw groups. Do not interchange lengths. Confirm quantities and locations from the BOM, hole depth, and video before tightening; a screw must not press against the PCB, battery, or moving mechanism.

## 2. Complete assembly video {#video}

<video controls playsinline preload="metadata" poster="/media/docs/assembly/assembly-poster.jpg" style="width: 100%; border-radius: 12px;">
  <source src="https://github.com/orulink-ai/WatcheRobot-public/releases/download/wiki-media-assembly-20261028/watcherobot-assembly-tutorial-720p-7cd0f58e.mp4" type="video/mp4">
  This browser cannot play the video. Use the chapter timeline below to locate the content.
</video>

The video is about 9 minutes 54 seconds. This preview is compressed to 720p and keeps the original picture content unchanged.

| Time | Stage |
| --- | --- |
| 00:00 | Inventory the parts and prepare the body mechanism and servos |
| 00:20 | Install the internal body servo and linkage parts |
| 01:20 | Secure the bottom structure and related small PCB |
| 02:00 | Install the bottom power PCB and external connector |
| 03:20 | Connect the Watcher head to the body control board |
| 04:30 | Organize the head, body, and control-board harnesses |
| 05:00 | Install the body shell, touch foil, and FPC cable |
| 05:30 | Align the enclosure holes and secure the long screws |
| 06:20 | Close the body structure and position the control board |
| 07:00 | Connect the control board, servos, and remaining harnesses |
| 08:20 | Install the battery and organize the internal space |
| 09:00 | Install the rear and bottom covers and inspect the enclosure |

The timestamps are navigation aids. Follow the continuous video and BOM for the actual action.