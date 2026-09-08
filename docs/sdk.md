<p><strong>English</strong> | <a href="sdk_zh.md">简体中文</a></p>

# SDK Installation and Commands

Complete firmware flashing and SD preparation first. These steps set up Python control from your computer.

## 1. Prepare the Download Folder

Download the `watcherobot` `.whl` from the [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest). Put it in a separate folder with only this wheel. Use a Python version supported by the Release.

Open a terminal in that folder. On Windows, type `powershell` in its address bar and press Enter.

## 2. Create the Environment and Install

Windows PowerShell, one line at a time:

```powershell
python --version
python -m venv .venv
$wheel = Get-Item .\watcherobot-*.whl
.\.venv\Scripts\python.exe -m pip install $wheel.FullName
.\.venv\Scripts\watcherobot.exe --version
```

macOS/Linux, one line at a time:

```sh
python3 --version
python3 -m venv .venv
.venv/bin/python -m pip install ./watcherobot-*.whl
.venv/bin/watcherobot --version
```

The final command prints the SDK version. Activation is not required; use the programs inside this folder for the following steps.

## 3. Connect the Robot

For first-time network setup, use a Windows or macOS computer with Bluetooth and follow the prompts:

```powershell
.\.venv\Scripts\watcherobot.exe robot setup
```

If the robot already has Wi-Fi, connect the computer to the same network and open the Python SDK app on the robot. Replace `123456` below with the displayed six-digit pairing code:

```powershell
.\.venv\Scripts\watcherobot.exe robot pair 123456
.\.venv\Scripts\watcherobot.exe robot status
```

On macOS/Linux, replace `..venvScriptswatcherobot.exe` with `.venv/bin/watcherobot`. Once status returns the device information, you can use the SDK.

## 4. Run Your Python Script

Save your script in this folder. For a script named `my_robot.py`, run on Windows:

```powershell
.\.venv\Scripts\python.exe .\my_robot.py
```

macOS/Linux:

```sh
.venv/bin/python ./my_robot.py
```

`my_robot.py` is an example filename; replace it with your saved script. See the [SDK README](../python-sdk/README.md) for API usage and examples, including any extra example dependencies.
