English | [简体中文](sdk_zh.md)

# Python SDK Quick Start

Build a managed Application on your computer to control the robot, without flashing custom ESP32 firmware. This guide targets SDK **0.1.12**, with Python **3.10–3.12** (3.11 recommended). The public repository's `python-sdk/` may pin an older revision; it does not identify your installed package.

## 1. Install the SDK

Install [Miniconda](https://docs.conda.io/projects/miniconda/en/latest/) and open a Conda-enabled terminal (Anaconda PowerShell Prompt on Windows):

```sh
conda create -n watcherobot-sdk python=3.11 pip -y
conda activate watcherobot-sdk
python -m pip install --upgrade pip
python -m pip install watcherobot
watcherobot --version
```

For a `.whl` from [Releases](https://github.com/orulink-ai/WatcheRobot-public/releases), run `python -m pip install "full-path-to-downloaded.whl"` in the same environment instead. Avoid competing installations. Check that the package supports the required APIs; upgrading it does not upgrade an already-running Desktop Daemon.

As checked on 2026-10-15, the latest SDK GitHub release is **0.1.11**; source version **0.1.12 has no corresponding official package yet**. To reproduce this guide's source baseline (including the new voice credential wizard), install [Git](https://git-scm.com/downloads), then use this instead of the release package in your dedicated environment. This is a pinned source installation, not an official release:

```sh
python -m pip install "watcherobot @ git+https://github.com/orulink-ai/WatcheRobot_python_sdk.git@cc0a31993b4fe1a607e75ad920ad013e5ddc934a"
watcherobot --version
```

## 2. Connect and pair

Follow the [device guide](../wiki/guides/en/products/watcherobot/device-guide.md#wifi) for phone QR provisioning. Connect the computer and robot to the same LAN, open **Desktop Link**, and read its current six-digit code:

```sh
watcherobot robot pair 123456
watcherobot robot status
```

Replace `123456` with the actual code and confirm a connected state. Older firmware may call this mode **Python SDK**.

Without a phone, run `watcherobot robot setup` on a Bluetooth-capable Windows/macOS computer and follow its prompts. Phone QR provisioning does not require computer Bluetooth.

## 3. Create and run an Application

```sh
watcherobot app init hello_robot
cd hello_robot
watcherobot app check .
watcherobot app run
```

The base template starts the application lifecycle; it does not move the robot automatically. Edit generated `app.py` and use `app.robot` inside `ApplicationContext.from_environment()`. Use `app.desktop` for desktop messages. Run synchronous device calls in `asyncio.to_thread(...)` to avoid blocking asynchronous channels.

**Start with `watcherobot app run`, not `python app.py`.** The Daemon owns connections and application lifecycle; the Application owns business logic. Passing `app check` validates the project, not hardware or cloud services.

## 4. Continue developing

- [Robot capabilities](../wiki/guides/en/products/watcherobot/capabilities.md): camera, audio, motion, lights, expressions, and inputs.
- [Create a voice conversation](../wiki/guides/en/products/watcherobot/voice.md): the built-in ASR, LLM, and TTS template.
- [SDK and Application architecture](../wiki/guides/en/sdk/index.md).
- [SDK 0.1.12 source and full reference](https://github.com/orulink-ai/WatcheRobot_python_sdk/tree/cc0a31993b4fe1a607e75ad920ad013e5ddc934a).

For problems, record `watcherobot --version`, `watcherobot daemon status`, and `watcherobot robot status`. Do not launch competing Daemon versions; stopping or switching runtimes affects the active application.
