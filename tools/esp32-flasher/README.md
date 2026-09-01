# ESP32 Flasher Command Reference

The full flashing flow is documented in `docs/flashing.md`. This file is only the command reference for the Python release ZIP helper exposed as `tools.win_flasher`.

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
python -m tools.win_flasher flash --zip .\WatcheRobot-ESP32S3-v0.3.2.zip --port COM7 --monitor
```

## Windows Shortcut

```powershell
tools\flash-release.cmd --zip .\WatcheRobot-ESP32S3-v0.3.2.zip --port COM7 --monitor
```

## AI-Assisted Flashing

For AI-assisted flashing, ask the assistant to read `tools/flashing/README.md` first. The AI should still use the same `python -m tools.win_flasher` command underneath, then inspect the boot log and report the result.
