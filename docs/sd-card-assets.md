English | [简体中文](sd-card-assets_zh.md)

# SD-card Resources

## 1. Prepare the SD card

Connect the card through a card reader. Use **FAT32 with a 512-byte allocation unit**. A card already using these settings does not need reformatting.

On Windows, right-click the SD card in **This PC → Format**, select **FAT32** and **512 bytes**, then click **Start**. **Formatting deletes all files on the card, including existing resources and creator works. Back them up first.**

Alternatively, activate the dedicated Conda environment in an Administrator PowerShell terminal and run this formatter from the repository root. Replace `E:\` with the actual SD card:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd-format --drive "E:\"
```

Check the displayed drive and capacity, then type `E:` to confirm or press Enter to cancel. The formatter supports Windows removable drives only and verifies FAT32 and the 512-byte allocation unit afterward. It stops with an error if Windows cannot format the selected capacity with these settings.

## 2. Write resources

The SD resource package is a release archive; the writer converts it into the device layout. Activate the dedicated Conda environment and run from the repository root:

```powershell
# Windows: download the latest official resources
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd --drive "E:\"
```

```sh
# macOS/Linux: download the latest official resources
bash tools/flash.sh sd --drive "/Volumes/WATCHE"
```

If `watche-sd-resources-*.tar.gz` was downloaded from the Release, append `--package "archive path"` to the matching command. The writer verifies the archive, preserves existing works, and installs this device layout:

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

The final `Installed ... successfully` message confirms the computer-side write and verification. Eject the card safely and reinsert it while the robot is powered off. Device-side acceptance is complete after a normal boot and successful playback of one expression.
