<p><strong>English</strong> | <a href="flashing_zh.md">简体中文</a></p>

# Assembly and Flashing

Follow this order: **assemble → flash the body board → flash the Watcher head → prepare the SD card → power on**.

## 1. Download and Prepare

Open [Releases](https://github.com/orulink-ai/WatcheRobot-public/releases) and download these three attachments from the release marked Latest:

- ZIP containing `STM32`: firmware for the board inside the body.
- ZIP containing `PTL-paired`: Himax and ESP32 firmware and scripts for the Watcher head.
- Archive containing `sd-resources`: files for the SD card.

Choose files by the versions listed on the page. No manual checksum step is required. See the [OSHW project](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl) for materials and assembly.

Install Python 3.10+ on the computer. On Windows, enable the option to add Python to PATH and reopen the terminal after installation. You also need ST-LINK, OpenOCD, the CH342 driver, a USB data cable, and an SD card reader. A firmware build environment is not required.

## 2. Flash the STM32 Board Inside the Body

1. Complete assembly, power off, check wiring, and connect ST-LINK to the body board's labeled SWD pins.
2. Supply power as specified for the board and connect ST-LINK to the computer.
3. Extract the STM32 ZIP and enter the folder containing `watcheRobot_STM32.bin`. Open a terminal there. On Windows, type `powershell` in the folder's address bar and press Enter.
4. Run:

```text
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program watcheRobot_STM32.bin verify reset exit 0x08000000"
```

After `Verified OK` and successful completion, power off before removing ST-LINK. If `openocd` is not found, add its bin directory to PATH and reopen the terminal.

## 3. Create the Python Environment for the Head

Extract the `PTL-paired` ZIP. Open the folder that contains **both `requirements.txt` and `tools`**, then open a terminal there. Run all remaining flashing commands from this folder.

Windows PowerShell, one line at a time:

```powershell
python --version
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
```

macOS/Linux, one line at a time:

```sh
python3 --version
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

The commands check Python, create a dedicated environment in this folder, and install the script dependencies. Wait for installation to succeed. Later commands use the environment's Python directly; **activation is not required**.

## 4. Identify the Head's Two Ports

Connect the Watcher head with a USB data cable. In the same terminal, run:

Windows:

```powershell
.\.venv\Scripts\python.exe -m serial.tools.list_ports -v
```

macOS/Linux:

```sh
.venv/bin/python -m serial.tools.list_ports -v
```

Find the two ports on the same CH342 device: **SERIAL-B / MI_02 is the ESP32 control port; SERIAL-A / MI_00 is the Himax port**. Windows Device Manager port properties can help identify them. Do not infer roles from COM numbers. If both ports are not present, check the cable and driver.

## 5. Run the Head Flashing Script

This Windows example assumes COM5 is the control port and COM6 is the Himax port. **Replace both with the ports identified above before running it.**

```powershell
.\.venv\Scripts\python.exe tools\ptl_release.py flash --port COM5 --vision-port COM6
```

On macOS/Linux, replace both port paths:

```sh
.venv/bin/python tools/ptl_release.py flash --port /dev/CONTROL_PORT --vision-port /dev/VISION_PORT
```

The script automatically checks the package files, then flashes Himax followed by ESP32. Keep the cable connected until `PTL paired flash completed.` appears. If a step fails, stop and retain the error message for troubleshooting.

### Optional: Use the Skill with an AI Assistant

The Skill is an instruction file for an AI assistant, not a Python script. Open the extracted package folder in an AI coding assistant and ask:

> Read skills/watche-ptl-release-flash/SKILL.md, create the flashing environment, identify the connected Watcher's two ports, run the paired flash, and check the result.

No separate Skill installation is needed for the manual steps above.

## 6. Prepare the SD Card and Power On

1. Power off, remove the SD card from the Watcher head, and connect it to the computer through a card reader. Use a card reader to prepare the SD card.
2. Back up files you want to keep, then format the card as FAT32.
3. Extract the contents of the `sd-resources` archive to the card root. Do not copy the archive itself or add an enclosing folder.
4. Confirm that `assets/`, `official_catalog.json`, and `resource_manifest.json` are at the root.
5. Safely eject the card, insert it into the powered-off head, then power on.

Install the client for your operating system from the Release and follow [First-Start Checks](action-test.md). For Python control, continue with [SDK Installation and Commands](sdk.md).
