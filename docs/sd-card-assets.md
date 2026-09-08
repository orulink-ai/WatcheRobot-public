<p><strong>English</strong> | <a href="sd-card-assets_zh.md">简体中文</a></p>

# SD-Card Behavior Assets

WatcheRobot device resources are distributed as the `sd-resources` `.tar.gz` asset in the release marked **Latest**. The public repository does not include the original resource-generation source.

## Required Environment

- A FAT32 SD card supported by the robot
- An SD card reader
- A tool that can extract `.tar.gz` archives
- The `sd-resources` `.tar.gz` archive from the Release

## Expected SD Layout

```text
<sd-card-root>/
  assets/
    actions/
    anim/
    ...
  official_catalog.json
  resource_manifest.json
```

The exact set depends on the release version.

## Release Path

Prepare the card in this order:

1. Remove the card from the Watcher head and insert it into a card reader connected to the computer. The Watcher USB and serial ports cannot be used to write this card.
2. Download the SD resource archive from the Release page.
3. Back up any files that must be retained from the SD card, then format it as FAT32.
4. Extract the contents of the `sd-resources` `.tar.gz` archive directly to the card root. Do not copy the archive itself and do not add another enclosing directory.
5. Confirm that `assets/`, `official_catalog.json`, and `resource_manifest.json` are directly under the card root.
6. Safely eject the card reader, remove the card, insert it into the powered-off Watcher head, and then power on.
7. Run the smoke test in `docs/action-test.md`.

## Verification Checklist

- `assets/anim/` contains `.animpack` files.
- `assets/actions/` contains action resource files.
- `official_catalog.json` and `resource_manifest.json` exist at the card root.
- The SD card is safely ejected before inserting it into the robot.
- The first action smoke test can trigger a behavior without missing-asset errors.

## Failure Notes

If resources are not detected, check for an accidental extra directory level, confirm the card is FAT32, and download the archive again if extraction fails.
