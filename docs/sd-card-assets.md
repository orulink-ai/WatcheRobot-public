# SD-Card Behavior Assets

WatcheRobot behavior animations are stored on the SD card as generated AnimPack assets. The public repository does not include the original firmware asset-generation source; use the SD-card asset ZIP from the same GitHub Release bundle as the firmware.

## Expected SD Layout

```text
<sd-card-root>/
  anim/
    anim_manifest.bin
    boot.animpack
    happy.animpack
    standby.animpack
    ...
```

The exact set depends on the release version.

## Release Path

When a GitHub Release provides an SD-card behavior asset ZIP:

1. Format the SD card using the filesystem required by the released firmware.
2. Extract the ZIP at the SD-card root.
3. Confirm `anim/anim_manifest.bin` exists.
4. Insert the card before booting the robot.
5. Run the smoke test in `docs/action-test.md`.

## Verification Checklist

- `anim/anim_manifest.bin` exists.
- At least one `.animpack` exists for `boot`, `standby`, or another expected behavior.
- The SD card is safely ejected before inserting it into the robot.
- The first action smoke test can trigger a behavior without missing-asset errors.

## Failure Notes

If an animation is missing, first confirm that the SD-card asset ZIP and firmware ZIP came from the same Release. If they do not match, re-copy the assets from the correct bundle before debugging hardware.
