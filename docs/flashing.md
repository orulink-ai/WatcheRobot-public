English | [简体中文](flashing_zh.md)

# Flashing and First Use

See the [OSHW project](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl) for parts and assembly. Once hardware is ready, follow the steps below.

## 1. Prepare Materials, Files, and Environment

Have the STM32 body board, Watcher head, ST-LINK V2, USB data cable, SD card, and card reader ready. Disconnect power before changing wiring or inserting/removing the SD card.

Download and extract these assets from the [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest):

| Asset name contains | Purpose |
| --- | --- |
| STM32 | Body board firmware |
| PTL-paired | Head Himax and ESP32-S3 firmware and paired tools |
| sd-resources | SD card expressions, actions, and other resources |

Use Conda, or install [Miniconda](https://docs.conda.io/projects/miniconda/en/latest/) first. Open a Conda-enabled terminal at the repository root (Anaconda PowerShell Prompt on Windows). Create the dedicated environment once:

```sh
conda create -n watcherobot python=3.12 pip -y
conda activate watcherobot
```

Run the following commands at the repository root, replacing firmware paths with the extracted folders. The script prepares dependencies and flashing tools; initial setup requires internet access.

## 2. Flash the STM32 Body Board

Connect ST-LINK V2 to the four-pin connector on the board inside the body. Follow the labels, not wire colors.

<img src="images/flashing/stm32-swd-wiring.png" alt="STM32 body board to ST-LINK V2 wiring" width="620">

| Body board | ST-LINK V2 |
| --- | --- |
| SIO | SWDIO |
| SCK | SWCLK |
| GND | GND |
| 3V3 | 3.3V, subject to the actual power arrangement |

Never connect 5V to 3V3. Do not parallel the ST-LINK 3.3V power output with an externally powered board. Check wiring and power, then connect ST-LINK to the computer.

Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 stm32 --package "extracted-STM32-folder"
```

macOS/Linux:

```sh
bash tools/flash.sh stm32 --package "extracted-STM32-folder"
```

Wait for `Programming Finished`, `Verified OK`, and `Resetting Target` with successful completion. Disconnect power and remove ST-LINK before continuing. If the chip cannot be reached, disconnect power and check wiring at both ends.

## 3. Flash Head Himax and ESP32-S3

Connect the Watcher head with a USB data cable. First run this command to install dependencies and list ports; it does not write firmware:

Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 head --package "extracted-PTL-paired-folder"
```

macOS/Linux:

```sh
bash tools/flash.sh head --package "extracted-PTL-paired-folder"
```

Identify the ports corresponding to SERIAL-B and SERIAL-A in the output. On Windows, the `desc:` names map to these arguments; COM61 and COM62 are examples:

| Device name | Port (example) | Command argument |
| --- | --- | --- |
| USB-Enhanced-SERIAL-B CH342 (ESP32) | COM61 | `--port COM61` |
| USB-Enhanced-SERIAL-A CH342 (Himax) | COM62 | `--vision-port COM62` |

Alternatively, open Windows Device Manager → Ports (COM & LPT) and read the COM number in parentheses after each name. Connect only one Watcher for flashing.

Use SERIAL-B for `--port` and SERIAL-A for `--vision-port`. Replace both port numbers below with your actual values before running:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 head --package "extracted-PTL-paired-folder" --port COM61 --vision-port COM62
```

Full macOS/Linux flashing command:

```sh
bash tools/flash.sh head --package "extracted-PTL-paired-folder" --port "/dev/SERIAL_B" --vision-port "/dev/SERIAL_A"
```

`/dev/SERIAL_B` and `/dev/SERIAL_A` are placeholders. Replace them with the identified ESP32 and Himax serial paths respectively; do not infer the mapping from port number order. Do not flash until the mapping is confirmed.

The script flashes Himax first, then ESP32-S3; no separate commands are needed. After `HX flash completed; reboot accepted.`, keep waiting until `PTL paired flash completed.` confirms the whole step. Do not unplug the cable during flashing.

Stop if ports are missing or inaccessible: automatic CH342 driver installation and Linux serial permission setup are not yet supported.

## 4. Prepare the SD Card with a Card Reader

Connect a FAT32-formatted SD card to your computer with a card reader, extract the SD resource archive to the card root, then reinsert the card into the head. See [SD-card Resources](sd-card-assets.md) for the root contents.

## 5. Power On and Use

Check the SD card and wiring, connect power, and press the power button. Check that the normal screen appears; the automatic reset after flashing may retain the powered-off state. Download Desktop or Android installers from the Release; use its TestFlight link for iOS. Python users can follow the [SDK Guide](sdk.md).

After connecting, follow the [First-run Check](action-test.md) with an expression, a light effect, and a movement within a safe range.

## Use the Skill (Optional)

Open this repository in an AI coding assistant and ask:

> Read skills/watche-release-flash/SKILL.md, use the dedicated Conda environment, confirm the firmware folder and connected target, then flash and report the actual result.

Without an AI assistant, simply use the commands above.
