English | [简体中文](sd-card-assets_zh.md)

# SD-card Resources

The SD resource package is a release archive and must not be unpacked directly at the card root. Connect a FAT32 SD card through a card reader, activate the dedicated Conda environment, and run from the repository root:

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
