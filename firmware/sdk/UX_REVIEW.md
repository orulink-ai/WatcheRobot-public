# Hardware example UX review

## Documentation closeout: three perspectives (2026-10-07)

- Beginner: "Does build success mean I can flash? Can I copy ESP32_PORT?"
  Engineering: build is host-only; pairing/backup gates precede flashing, and
  every port placeholder must be replaced. Product: show this branching path
  once, then retain step-by-step instructions and a stop condition.
- Product: "How do I know Photo or Record worked?"
  Engineering: request/ESP_OK is not physical acceptance; Record and Play are
  independent, and incomplete recordings cannot be played. Beginner: show
  countdown, expected status, actual result and a concrete failure next step.
- Engineering: "Will simplifying hide ownership or make a second build system?"
  Product: keep beginner tutorials, experiment how-to, interface reference and
  acceptance evidence separate. Preserve standard ESP-IDF layout and commands.
  Architecture diagrams show the optional SDK modules and the STM32/Himax
  boundaries; application policy stays outside hardware drivers.

The structure was checked against Espressif's IDF 6.0.2 Get Started and
Build System guides. This was a documentation comparison, not a clean-machine
walkthrough. Chinese/English guides have the same operational and safety scope.

### Blockers

None found for this local development-preview commit. Stable release gates
remain open; this is not a release approval.

### High Risk

None found in the scoped documentation additions. No code/flash instruction
bypasses pairing, valid-position motion gating or the existing resource owners.

### Medium/Low

Mermaid diagrams supplement text and commands rather than replacing them.
Viewers without Mermaid support can follow the numbered instructions. Troubleshooting
explains effective replay gain and that noise may become inaudible along with
speech at 1x; no automatic denoising or resolved squeal cause is claimed.

### Missing Verification

Clean-checkout/new-user execution, complete paired-firmware identifiers, Linux
and fresh CI, camera/UI quality, audio quality and integrated endurance remain
pending. The latest serial/limited hearing observation is in VALIDATION.md,
not treated as release acceptance or copied into the experiment instructions.

### No Finding Areas

Beginner navigation/local links, example/button names, IDF version and standard
commands, module call boundaries, separate Record/Play, immutable PCM scratch,
and documented error/resource ownership constraints were checked. Workflow and
architecture presence tests failed before the additions and passed afterwards.

## UX5 speaker-test review (2026-10-07)

### Blockers

None found in the scoped source-level review. Full hardware/release acceptance
is not granted; the missing verification below remains a release gate.

### High Risk

None found in the new player, lease-drain and mute paths. Existing I2S backend
reconfigure/short-write reporting limits remain: API success is delivery evidence,
not acoustic proof. No codec/PA gain cap or microphone analog gain was bypassed.

### Medium/Low

- The speech asset adds 121124 bytes of read-only PCM to head/robot flash, not body.
  Its CC0 provenance/checksum are recorded. Offline processing is not a raw SDK
  normalization policy. Private JoyInside/Microsoft diagnostic voices are excluded.
- No cancellation or dynamic UI/media teardown is added; resources persist until
  process exit. Failure recovery does not automatically replay a submitted prefix.
- Output remains 90 by default and capped at 95; 0 now actually mutes.
  Replay gain raises noise and only affects recording scratch, as before.

### Missing Verification

- Actual round-screen layout/readability, Voice/Tone buttons and audible sample.
- Hardware 0 mute, 90/95 comparison, repeated playback and drain-error recovery.
- Deliberate microphone recording/replay and input quality, still separate from
  the accepted same-source JoyInside/SDK output comparison.
- Camera preview/touch, stationary combination, long-duration stress, Linux
  host execution and fresh remote CI. Build/host results are not these checks.

### No Finding Areas

- Source bounds and partial final chunks; immutable recording/flash PCM, writable
  scratch, clipping counters and preserved recordings on failed playback.
- One media owner; audio hardware setters/drain outside LVGL callbacks and locks.
  Queue claims busy before dispatch; settings/actions remain disabled until drain.
- Injected write/drain failure precedence and no automatic prefix retransmission.
- HAL lease/codec/handle guards, positive timeout requirement, lock release on
  all returns and failure propagation. Mute/unmute tests use production functions.
- Portable checksum, sample count, endpoints and peak/RMS bounds on the fixed
  asset. Existing output error and SDK lifecycle tests remain enabled in Release.
- Application-level UX remains outside hardware SDK; canonical source and public
  delivery snapshot match. No cloud, storage, models or motion policy added.


## UX4 output-headroom review (2026-10-07)

- Blockers: None found in the scoped source-level changes; not release approval.
- High Risk: No hardware/PA-gain cap was bypassed. Acoustic loudness and distortion
  are not verified by PCM limits or successful writes and remain acceptance gates.
  Existing backend short-write/reconfigure-reporting limitations remain unchanged.
- Medium/Low: 16x/32x replay boost can raise noise substantially. Defaults remain
  1x; user opts in, multiplier is reduced against the entire raw recording peak,
  and signed saturation is the final guard. Tone increases to 24000; default
  output increases to 90, while the BSP distortion cap remains 95. Keep ears
  away from the speaker and reduce output if uncomfortable or distorted.
- Missing Verification: hearing comparison at 90/95, physical slider/step
  behavior, replay noise/quality, battery/current effects and long-duration use.
  No measured acoustic +11 dB claim: this is only the calculated digital-source
  and codec-curve change versus UX3 defaults.
- No Finding Areas: pure tone tests cover one-second duration, 10 ms fades,
  out-of-range silence and peak/RMS bounds. Boost tests cover 16x/32x, hostile
  peak values, safe reduction and immutable saved PCM. All source changes stay
  in the shared example policy; SDK/HAL, analog gain, I2S and PA ownership are
  untouched. Replay boost is labeled as Play-only, not whole-speaker volume.

Local tests passed in Debug/Release; public tools pytest passed 73 and internal
SDK/output pytest passed 18. Public head and robot built on IDF 6.0.2 Windows.
Actual UX4 hearing results must be recorded separately after manual interaction.
App-only write hash passed; bounded boot capture identifies UX4 and all three
head modules ESP_OK. No automatic tone/body action was invoked.

## UX3 follow-up review (2026-10-07)

- Blockers: no new static blocker found in the reviewed volume/boost change.
  This is not release acceptance.
- High Risk: audible output is still not verified for the regular 16 kHz UX3
  example. Existing I2S backend success reporting does not check all reconfigure
  errors / short writes; ESP_OK must not be interpreted as acoustic success.
- Medium/Low: replay boost also amplifies noise. It is explicit, defaults to 1x,
  and is reduced against the entire raw recording peak to prevent PCM overflow.
  Settings are RAM-only. Initialization needs restart on failure; no cancellation
  or dynamic UI/worker teardown was introduced.
- Missing Verification: physical round-screen fit, +/- and slider interaction,
  output zero mute / 80–85 comparison, deliberate speech capture and boosted
  replay, repeated memory stability and robot combination on hardware.
  Public head app-flash hash passed and UX3 boot initialized all three modules
  with ESP_OK. Previous manual 24 kHz HAL/SDK buttons were audible but quiet;
  that is not this regular 16 kHz example's acceptance.
- No Finding Areas: hardware setters stay outside LVGL callbacks/locks and run
  on the single media owner. Busy settings are disabled; applied volume cache
  changes only on setter success. Saved PCM remains immutable; scratch clamps
  signed extremes. No microphone analog gain, autonomous sound, body command,
  official application dependency or persistent preference storage was added.
  Host state/audio policy tests cover Debug and Release; documentation tests
  check worker handoff, source ownership and current UI settings.

The simple tool-oriented design keeps Photo/Record/Play primary and moves output
volume / replay boost to an Audio page with large +/- and Back buttons. A raw
logarithmic mic meter separates input level from output volume; no decorative
animation or hidden auto gain is used.

## Earlier UX2 evidence (historical)

## Blockers

None found in static review. This is not release or hardware acceptance.

## High Risk

None found in reviewed buffer ownership and task/lock paths. Camera callback copies
borrowed bytes; decoding is bounded; LVGL image cache is invalidated and old pixels
are freed only under its lock after replacement. Audio/camera share one media owner.

## Medium/Low

The example keeps its UI, worker, PCM and last preview until process exit; dynamic
shutdown is not exposed. Initialization failures require correcting wiring/pairing
and restarting. No operation cancellation is offered. These limits are documented.

## Missing Verification

Actual round-screen clipping/readability, physical clicks, JPEG orientation/color,
three-second audible recording/playback, repeated photos and memory stability,
missing PTL peer, audio faults, and robot touch dispatch while recording remain
hardware checks. A separate head-only diagnostic was app-flashed and run twice:
640x480 JPEG decoded successfully; 48,000 microphone samples were nonzero; tone
writes, playback and close returned ESP_OK. The operator heard no tone. This does
not validate audible output, and build/host tests do not replace those checks.
Do not label this UI as fully hardware validated.

UX2 adds separate Record/Play, visible photo countdown, a larger photo page, input
peak/mean diagnostics and an independent speaker tone. The host state test covers
Play-before-record, partial recording invalidation, retry/replay, busy gating,
bounded image dimensions/zoom and signed PCM extremes. Playback copies immutable
original PCM to a writable scratch block before the codec call. Default volume
30 maps to approximately -35 dB in the current driver; UX2 uses 60 (-20 dB).
This is a plausible contributor to quiet output, not a confirmed explanation of
the user's silent device. Passive access initially failed while a user monitor
held the port; it succeeded after the operator closed it. No monitor was killed.
The earlier combined-action application logged successful capture/playback despite
reported silence. During the later diagnostic, the 660Hz component in microphone
samples increased with tone output level. Electrical coupling cannot be excluded,
so this is not proof of acoustic output. The diagnostic's PA input-mask reading
does not measure its output pin and must not be cited as amplifier-power evidence.
The public interactive UX2 head example was subsequently app-flashed; hash
verification and display/camera/audio initialization passed. This does not
prove screen interaction or audible output. No body/motor API was exercised.

## No Finding Areas

Reviewed: one-action gating, distinct recording/playback failures, module isolation,
LVGL locks not spanning blocking media, image/PCM memory bounds and ownership,
independent application dependencies, no automatic motion or storage writes added.
