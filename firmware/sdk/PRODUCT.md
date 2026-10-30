# Hardware example product

## Users

New ESP32 developers, including beginners, using a finger on a desktop robot's 412 pixel screen in ordinary indoor light. They need visible proof that a hardware operation started and completed without watching a serial terminal.

## Product Purpose

Demonstrate independent firmware using hardware APIs. A photo produces a tappable
preview after a visible countdown; recording and playback are separate deliberate
actions. Input levels and a speaker test help diagnose silence without claiming
hardware success from API results. Preserve hardware safety and module isolation.

## Brand Personality

Clear, calm, practical. English first for public open-source users, confirmed by the user. Tool-like rather than decorative robot expression.

## Anti-references

Silent buttons, success inferred from API submission, decorative animations, expression systems, inaccessible status conveyed only by color, large app frameworks for a small example.

## Design Principles

Show actual results. Confirm actions immediately. Disable conflicting actions while busy. Explain failures with a next step. Keep application UI outside the SDK and share it between examples.

The guided robot hardware check uses generous whitespace and two bottom navigation
actions only: Back and Test, with the latter changing to Stop or Next as the task
progresses. No persistent utility-button grid. Technical evidence lives outside the
main task page; motion safety confirmation and Stop remain explicit. Existing short
head/body teaching examples retain their own layouts until deliberately migrated.
The browser design and hardware_check v2 expand these actions into separated lower-left /
lower-right curved touch regions with a neutral gap. Pale action surfaces keep
the result prominent; danger STOP stays red, including while stopping is pending.
A side-region alternative remains available outside the device UI.
The LVGL layout is implemented and was flashed with startup verification on
2026-10-09; physical acceptance still requires operator testing.
The hardware_check replay policy requests a fixed
32x, peak-limited copy gain, without a multiplier button or any SDK gain policy.

## Accessibility & Inclusion

Large touch targets, readable short English labels, text plus semantic color. No decorative motion. No dependency on Chinese glyphs or SD resources. Audible quality and physical touch remain explicit hardware acceptance items.
