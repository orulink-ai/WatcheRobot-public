# Python SDK overview

`watcherobot` is the official Python SDK and includes the WatcheRobot Runtime/Daemon. Developer programs run as managed Applications and receive Desktop and Device channels from the Daemon.

## Requirements

- Python 3.10–3.12; Python 3.11 is recommended.
- Windows or macOS with Bluetooth for first-time provisioning.
- The computer and robot on the same local network.
- A powered robot running compatible firmware.

## Install

```bash
python -m venv .venv
python -m pip install --upgrade pip
python -m pip install watcherobot
```

Activate the environment, then confirm the CLI:

```powershell
# Windows PowerShell
.\.venv\Scripts\Activate.ps1
watcherobot --version
```

```sh
# macOS / Linux
source .venv/bin/activate
watcherobot --version
```

## Pair a robot

```bash
watcherobot robot setup
```

If Wi-Fi is already configured, open the **Python SDK** Application on the robot and use its six-digit code:

```bash
watcherobot robot pair 123456
watcherobot robot status
```

## Create an Application

```bash
watcherobot app init hello_robot
cd hello_robot
watcherobot app run
```

Always start an Application with `watcherobot app run`, not `python app.py`. The Daemon must inject the device and desktop channels.

For the complete CLI, media APIs, Marketplace contract, and examples, see the versioned documentation in the [WatcheRobot Python SDK repository](https://github.com/orulink-ai/WatcheRobot_python_sdk).
