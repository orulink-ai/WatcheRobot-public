English | [简体中文](sd-card-assets_zh.md)

# SD-card Resources

For script setup, see [Prepare Materials, Files, and Environment](flashing.md#1-prepare-materials-files-and-environment).

## 1. Prepare the SD card

Power off the robot, remove the SD card, and connect it through a card reader. Use **FAT32 with a 512-byte allocation unit**. A card already using these settings does not need reformatting.

On Windows, right-click the SD card in **This PC → Format**, select **FAT32** and **512 bytes**, then click **Start**. **Formatting deletes all files on the card, including existing resources and creator works. Back them up first.**

Alternatively, activate the dedicated Conda environment in an Administrator PowerShell terminal and run this formatter from the repository root. Replace `E:\` with the actual SD card:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd-format --drive "E:\"
```

The command formats immediately without an additional confirmation prompt; check the drive letter and back up files before running it. The formatter supports Windows removable drives only and verifies FAT32 and the 512-byte allocation unit afterward. It stops with an error if Windows cannot format the selected capacity with these settings.

## 2. Write resources

Activate the dedicated Conda environment and run from the repository root. Replace the drive or mount path below with the actual card. Without `--package`, the writer downloads the latest official resources:

```powershell
# Windows: download the latest official resources
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd --drive "E:\"
```

```sh
# macOS/Linux: download the latest official resources
bash tools/flash.sh sd --drive "/Volumes/WATCHE"
```

If `watche-sd-resources-*.tar.gz` was downloaded from the Release, append `--package "archive path"` to the matching command. The writer verifies the archive and preserves existing works.

The final `Installed ... successfully` message confirms writing and verification. Eject the card safely, return it to the powered-off head, then follow [step 5 of the flashing guide](flashing.md#5-power-on-and-use) to start and check the robot.

## Installed layout

The writer converts the archive into this device layout; no manual extraction is needed:

```text
SD-card root/
└─ watche/
   ├─ assets/
   │  ├─ actions/                 Action objects
   │  ├─ anim/                    Expression animation objects
   │  └─ sfx/                     Sound objects
   ├─ official/current/
   │  ├─ official_catalog.json    Official resource catalog
   │  ├─ fixed_states.json        Fixed-state mapping
   │  └─ resource_manifest.json   Resource integrity manifest
   ├─ works/
   │  └─ works_catalog.json       Creator-work catalog
   ├─ system/
   │  ├─ layout.json              Layout marker
   │  └─ accepted_official.json   Installed official-resource record
   └─ staging/                    Installation transaction workspace
```
