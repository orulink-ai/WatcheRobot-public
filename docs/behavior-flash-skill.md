<p><strong>English</strong> | <a href="behavior-flash-skill_zh.md">简体中文</a></p>

# Behavior Asset Field Checklist

Use this checklist when preparing or replacing a WatcheRobot SD card from a public Release bundle.

## Inputs

- WatcheRobot repository checkout
- SD card connected to the host computer through a card reader
- the `sd-resources` `.tar.gz` archive from the release marked **Latest**

## Procedure

1. Remove the card from the Watcher head, insert it into a card reader, and confirm its mount path. Do not use the Watcher USB or serial ports to write the card.
   - Windows example: `E:\`
   - macOS example: `/Volumes/WATCHER_SD`
   - Linux example: `/media/$USER/WATCHER_SD`

2. Extract the downloaded `.tar.gz` to a temporary folder.

3. Confirm the extracted output.

```text
assets/
official_catalog.json
resource_manifest.json
```

4. Format the card as FAT32, then copy the extracted contents to the SD-card root without adding another enclosing directory.

5. Safely eject the SD card.

6. Remove the safely ejected card from the reader and insert it into the powered-off Watcher head.

7. Run `docs/action-test.md`.

## Pass Criteria

- SD card root contains `assets/`, `official_catalog.json`, and `resource_manifest.json`.
- `assets/anim/` contains `.animpack` files.
- Robot boot does not report missing animation assets.
- At least one behavior can be triggered during the first action smoke test.

## Failure Notes

If resources are missing, check the FAT32 filesystem and directory depth before debugging hardware.
