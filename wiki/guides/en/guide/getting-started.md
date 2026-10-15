# Getting started

Follow this path when setting up a WatcheRobot for the first time. At the end, Watcher Desktop should recognize the robot and you can use an official Application or continue with SDK development.

## Before you begin

- Keep the robot powered and near the computer.
- First-time provisioning requires Bluetooth on Windows or macOS.
- After provisioning, connect the computer and robot to the same local network.
- Do not run multiple Watcher Desktop or SDK Runtime instances during setup.

## 1. Install Watcher Desktop

Download the installer for your operating system from [Watcher Desktop Release Assets](https://github.com/orulink-ai/WatcheRobot-public/releases/latest).

Watcher Desktop owns the user interface and supervises the local runtime. The bundled SDK Daemon owns pairing, device connections, and Application lifecycle.

## 2. Provision the network

Open **Settings > Wi-Fi** on the robot and follow the on-device instructions. Once credentials are saved, the robot connects to the local network.

## 3. Pair the robot

1. Open Watcher Desktop.
2. Confirm that the displayed device identity matches the robot.
3. Open the matching connection entry on the robot.
4. Enter the six-digit pairing code shown on the robot.
5. Wait until the client reports a connected state.

See [Client and device](./client-and-device.md) for the connection model and state meanings.

## 4. Choose the next path

- Read [Firmware version management](./firmware-version-management.md) before upgrading or downgrading.
- Open the [Python SDK overview](../sdk/index.md) to build an Application.
- Start with [Troubleshooting](../troubleshooting/index.md) if the connection fails.
