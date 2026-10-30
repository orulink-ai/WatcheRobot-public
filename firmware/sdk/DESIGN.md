# Hardware example design

## Scene and theme

A beginner touches a small round desktop screen in daylight while checking hardware. A warm light surface improves legibility; no decorative animation.

## Colors

Warm surface #F4F1EC, muted preview #E6EAE5, dark text #293732, active green #247568, error red #B34132. Restrained accent reserved for actions, progress and status. State always has text.

## Layout

The guided robot check follows the two-button design in
`design/hardware-check/DESIGN.md`: progress, a short title, one central result,
one short status and Back / Test (or Stop / Next). Its whitespace-first layout
does not include persistent Details, Skip, Results or Audio buttons. Safety uses
a separate deliberate preparation page. Review uses Not OK / Next in the same two
slots; failure retains Issue, report details allow on-screen retest. The prototype
has 26 individually confirmed subtests. hardware_check v2 implements the
browser's 178x124 lower corner regions with
mirrored 112x100 inner curves, pale action surfaces, a minimum 56px neutral gap
and x=206 content axis. Labels are mirrored at (108,340)/(304,340), 18px.
Danger STOP remains red through pressed/pending states. A rounded side-region
alternative is browser-only. The lower-corner LVGL layout was flashed and its
startup checked on 2026-10-09; physical acceptance remains pending. Drawing and native hit testing share the same
pixel-centre circle/ellipse predicate, with cached scanline bounds and no frame-sized
touch-area bitmap. UI callbacks still only queue intentions.
Essential text remains at least 14px. Enlarged photos fit within 320x180,
without a redundant large title, with 10px clearance above the large
regions. In the browser, visible SVG path bounds, not just their viewports, are centred.
The firmware uses native LVGL symbols; its central visual and image axis are
(206,188), not a claim of identical browser/device glyphs. The UX5 layout below describes the
existing head teaching example, not the new check prototype.

412x412 reference screen, centered content inside its circular safe area. UX5 title,
operation state, image or large countdown, concise result/detail, progress, three
primary actions and secondary Test speaker / Audio actions. Preview retains aspect ratio.
Photo tap opens a larger in-page view with Back, not a confirmation modal.

## Typography and actions

LVGL Montserrat 20 title, 16 button labels and 48 countdown. English copy. Photo,
Record and Play have identical geometry. Busy state disables actions; Play also
requires a complete recording. Unavailable modules are disabled independently.

## Interaction contract

Photo: large 3/2/1 countdown, capturing, decoding, tappable preview or failure.
Keep bounded native pixels (maximum 640x480), fit to the thumbnail and larger
round-safe view; retain the last good image on failure and label it as previous.
Record: large countdown, three-second capture with live input peak, then stop.
Play: explicit user action, reusable immutable original PCM plus a writable scratch
block. Recording failure disables Play rather than exposing partial data.
Test speaker: a round-safe page with separate 52 px Test voice and Test tone
buttons. Voice plays bundled synthetic CC0 English speech; tone lasts one second
at 660 Hz. Both show Output, progress, finished/error, independent of microphone
or replay boost. Audio is reachable here; no automatic playback.
All playback shares a pure chunk/scratch/drain runner on the media owner.
Busy stays claimed until DMA drains; Back navigation is permitted during playback.
Default output setting 90, never automatic maximum volume. Completion means API
delivery, not proof of audible sound. UI/body dispatch keep running on their own
tasks. Buffers stay in RAM. No automatic sound or motor action at boot.

Audio settings: output slider 0–95 and +/-5 buttons, zero explicitly muted.
Hardware changes queue to the media owner, outside the LVGL lock; display the
applied value only on success. Busy operations disable settings. Replay boost
cycles 1x/2x/4x/8x/16x/32x, reduces its multiplier against the full recording peak, and
only changes scratch PCM. Noise may also rise. No hidden gain or auto normalization.
Replay boost is labeled explicitly: it changes recorded Play only, not either speaker test.
Output controls all speaker sound; the 95 distortion cap stays unchanged.
Raw input meter maps -60..0 peak dBFS, not acoustic sound pressure.
Manual tone uses peak 24000, 660 Hz and 10 ms fades. Preferences reset to 90/1x
on reboot; BSP microphone analog gain is unchanged.

The preceding adjustable-boost behavior describes head/robot. hardware_check
instead requests fixed 32x recording replay with whole-record peak limiting;
its setup page has no boost button. Output remains adjustable 0–95. The SDK is
still raw PCM, and voice/tone still use unity source gain.
