English | [简体中文](sd-card-assets_zh.md)

# SD-card Resources

Download the SD resource archive from the [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest). Connect a FAT32-formatted SD card through a card reader, extract the archive contents to the card root, and return the card to the Watcher head.

The current Release archive extracts to this structure:

```text
SD-card root/
├─ assets/
│  ├─ actions/          Action descriptions
│  ├─ anim/             Expression animations
│  └─ sfx/              Sound effects
├─ fixed_states.json
├─ official_catalog.json
└─ resource_manifest.json
```

These entries belong directly at the SD-card root; do not wrap them in an extra archive folder.
