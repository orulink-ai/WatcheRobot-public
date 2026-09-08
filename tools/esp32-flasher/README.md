<p><strong>English</strong> | <a href="README_zh.md">简体中文</a></p>

# ESP32 Flasher Command Reference

> Legacy ESP32-only helper: this tool does not flash Himax and must not be used with the current Release's `PTL-paired` ZIP. Extract that paired ZIP and follow its `skills/watche-ptl-release-flash/SKILL.md`.

The full current flashing flow is documented in `docs/flashing.md`. The commands below apply only to older ESP32-only ZIPs that contain `flash_args.txt`.

## Install Dependencies

```powershell
python -m pip install -r tools/win_flasher/requirements.txt
```

## List Local Release ZIPs

```powershell
python -m tools.win_flasher list-releases
```

By default, the helper scans `.local/release-zips/<version>/`. Release ZIPs are downloaded from GitHub Releases; they are not stored in this repository.

## List Serial Ports

```powershell
python -m tools.win_flasher list-ports
```

## Flash a Downloaded Release ZIP

```powershell
python -m tools.win_flasher flash --zip .\LEGACY-ESP32-PACKAGE.zip --port COM7 --monitor
```

## Windows Shortcut

```powershell
tools\flash-release.cmd flash --zip .\LEGACY-ESP32-PACKAGE.zip --port COM7 --monitor
```

## macOS/Linux Shortcut

Use the operating system's serial device, for example `/dev/cu.usbserial-*` on macOS or `/dev/ttyUSB0` on Linux:

```sh
./tools/flash.sh flash --zip ./LEGACY-ESP32-PACKAGE.zip --port /dev/ttyUSB0 --monitor
```

## AI-Assisted Flashing

For current AI-assisted PTL flashing, use the Skill and tools bundled in the paired ZIP instead of this helper.
