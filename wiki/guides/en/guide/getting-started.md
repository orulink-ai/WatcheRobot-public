English | [简体中文](../../guide/getting-started.md)

# Getting started

This page is for users with a working, assembled robot. Start with power and networking; source downloads and reflashing are not prerequisites. To build your own robot, follow the [DIY build guide](reproduction.md).

## 1. Power on and install a client

Use stable power and a safe, level surface. Find phone and desktop downloads in the [device guide](../products/watcherobot/device-guide.md#install-app), choosing the installer for your system.

## 2. Configure Wi-Fi with a QR code

First boot without saved credentials opens provisioning automatically. Alternatively open **Settings → Wi-Fi**. In the phone app, select **Scan QR code**, scan the robot, choose Wi-Fi, and enter the password. Wait for app success and Connected/IP on the robot.

See the [illustrated Wi-Fi guide](../products/watcherobot/device-guide.md#wifi). The phone needs Bluetooth and camera permissions; computer Bluetooth is not required. Do not repeatedly use Disconnect on a working network: it clears saved Wi-Fi.

## 3. Choose a connection

| Goal | Robot mode | Next step |
| --- | --- | --- |
| Computer client | Desktop Link | Same LAN; enter the six-digit robot code when prompted |
| Phone control | Phone Control | Tap Add Device in the app and match the Device ID |
| Develop an application | Desktop Link | Follow the [SDK quick start](../../../../docs/sdk.md) to pair and run an Application |

Pairing does not mean an application is running. See [Client and device](client-and-device.md) for connection states.

## 4. Verify and continue

Check one expression, light effect, and small movement in a safe area using the [first-run check](../../../../docs/action-test.md). On a computer, select and start an installed official or custom Application in the client.

- Update firmware: [flashing](../../../../docs/flashing.md) and [version management](firmware-version-management.md).
- Build applications: [capabilities](../products/watcherobot/capabilities.md) and [voice conversations](../products/watcherobot/voice.md).
- Connection issues: [troubleshooting](../troubleshooting/index.md); discuss with the [WeChat community](../products/watcherobot/community.md).
