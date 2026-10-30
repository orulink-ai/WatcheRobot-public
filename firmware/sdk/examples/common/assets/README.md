# Speaker-test asset

`speaker_pcm.inc` contains a synthetic English voice saying:

> This is the speaker test. One, two, three.

The original test text and generated sample data are dedicated to the public
domain under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
SPDX: `CC0-1.0`. This dedication applies to the asset, not surrounding SDK code.
No JoyInside response, private recording, Microsoft voice, MBROLA voice, voice
clone or cloud credentials are included. The sample is an intentionally robotic
formant voice, not an imitation of a person.

## Provenance and format

- eSpeak NG 1.52.0, built-in `en-us` formant voice, speed 150, amplitude 100.
- Input text is exactly the sentence above.
- Offline FFmpeg `N-121420-gce9d181444-20251011`: mono signed PCM16, 16000 Hz.
- Filter: `aresample=16000,acompressor=threshold=0.18:ratio=3:attack=5:release=80:makeup=2,afade=t=in:d=0.01`.
- The resulting master peak was 26866. Asset-generation scaling is
  `sign(sample) * round(abs(sample) * 30000 / 26866)` (round half away from zero).
  This is baked into the test asset, never applied automatically by the SDK.
- 60562 samples, 121124 bytes, 3.785125 seconds; peak 30000, RMS about 4191.
- SHA256 of canonical little-endian PCM bytes:
  `f76e5918bfba51b42c431ffab5d3d16ded679d765eb26e33c469bba1c4d5304f`.
- FNV-1a checksum and signal bounds are checked by the portable host test.

Generation uses eSpeak as an offline tool. Its GPL-3.0 engine/data and FFmpeg
are not linked into or distributed with this example. The formant synthesis
converts our own test text, not a third-party audio recording. See the
[GNU output FAQ](https://www.gnu.org/licenses/gpl-faq.html#GPLOutput).
Building or running the example requires neither tool, a network, nor a model.

The asset stays read-only in flash. The media owner copies each block into
writable scratch before SDK playback; repeated tests cannot compound codec
software gain. Speaker tests always use 1x source gain. Only the explicit Output
setting changes playback level; recording Replay boost does not affect tests.

This asset is not an acoustic calibration reference. Peak/RMS or successful
DMA delivery do not prove perceived loudness, speaker integrity or speech quality.
