# Behavior Asset Field Checklist

Use this checklist when preparing or replacing a WatcheRobot SD card from a public Release bundle.

## Inputs

- WatcheRobot repository checkout
- SD card mounted on the host machine
- SD-card asset ZIP from the same Release as the ESP32-S3 and STM32F103 firmware packages
- Target release version noted in `docs/versions.md`

## Procedure

1. Confirm the SD card mount path.
   - Windows example: `E:\`
   - macOS example: `/Volumes/WATCHER_SD`
   - Linux example: `/media/$USER/WATCHER_SD`

2. Extract the released SD-card asset ZIP to a temporary folder.

3. Confirm the extracted output.

```text
anim/anim_manifest.bin
anim/*.animpack
```

4. Copy the extracted `anim/` directory to the SD-card root. Replace any old `anim/` directory from another release.

5. Safely eject the SD card.

6. Insert the SD card into the robot before boot.

7. Run `docs/action-test.md`.

## Pass Criteria

- SD card contains `anim/anim_manifest.bin`.
- Robot boot does not report missing animation assets.
- At least one behavior can be triggered during the first action smoke test.

## Failure Notes

If an animation is missing, confirm that all firmware and SD-card resources came from the same Release bundle. Do not mix old local assets with newer firmware.
