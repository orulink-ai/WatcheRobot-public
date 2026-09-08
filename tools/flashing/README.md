<p><strong>English</strong> | <a href="README_zh.md">简体中文</a></p>

# AI-Assisted Firmware Flashing

The authoritative flashing Skill is inside the current Release's `PTL-paired` ZIP at `skills/watche-ptl-release-flash/SKILL.md`; it does not need separate installation.

Extract the paired ZIP, open that directory in an AI coding assistant, and ask:

```text
Read skills/watche-ptl-release-flash/SKILL.md, prepare the required environment,
verify the package, identify both CH342 interfaces, and follow the Skill.
```

Flash STM32F103 with ST-LINK/SWD before running the paired Himax and ESP32-S3 flow. See the [Firmware Flashing Guide](../../docs/flashing.md).
