English | [简体中文](sdk_zh.md)

# SDK Installation and Commands

Complete robot flashing, SD-card preparation, and network setup first. Download the `watcherobot` `.whl` from the [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest), then open a terminal in the folder containing it.

## 1. Create the Environment and Install

```bash
conda create -n watcherobot-sdk python=3.12 pip -y
conda activate watcherobot-sdk
```

Windows PowerShell:

```powershell
$wheel = Get-Item .\watcherobot-*.whl
python -m pip install $wheel.FullName
watcherobot --version
```

macOS/Linux:

```bash
python -m pip install ./watcherobot-*.whl
watcherobot --version
```

## 2. Set Up and Connect the Robot

For first-time network setup, run this on a Bluetooth-capable Windows or macOS computer:

```bash
watcherobot robot setup
```

After the robot is online, connect the computer to the same network and open the Python SDK app on the robot. Replace `123456` with the six-digit pairing code shown on screen:

```bash
watcherobot robot pair 123456
watcherobot robot status
```

## 3. Run a Python Script

```bash
python my_robot.py
```

Replace `my_robot.py` with your script file. See the [Python SDK README](../python-sdk/README.md) for APIs and examples.
