English | [简体中文](../../troubleshooting/index.md)

# Troubleshooting

Start with the smallest failure domain. Do not clear user data or erase the entire flash as a first response.

## QR provisioning fails

- Use Scan QR code inside the WatcheRobot app, not WeChat. Check phone Bluetooth and camera permissions.
- Wait for Bluetooth advertising if the device displays Preparing; this is not a QR or password error.
- After entering a password, wait for app success and Connected/IP on the robot. Retry incorrect passwords as prompted.
- A connected network-information page normally has no QR code. Disconnect clears Wi-Fi; use it only to change networks.
- Follow the [illustrated device guide](../products/watcherobot/device-guide.md#wifi).

## The client cannot find the robot

1. Confirm that the robot has completed startup and remains powered.
2. Put the computer and robot on the same local network.
   Open Desktop Link on the robot, not Phone Control.
3. Stop other Watcher Desktop or SDK Runtime instances.
4. Check whether the local firewall blocks network discovery.
5. Restart the client and inspect the device list again.

## Paired but disconnected

- Confirm that the robot did not move to another Wi-Fi network.
- Check whether the network enables client isolation.
- Allow automatic reconnection before deleting the pairing.
- Provision Wi-Fi again only after confirming that saved credentials are invalid.

## An Application does not start

A connected client does not imply that an Application is running. Confirm that the selected Application is compatible with the firmware and inspect the Application state and logs exposed by the client.

Third-party Applications may safely ignore unsupported desktop business frames, but must not require the Daemon to bypass the active Application and send them straight to the device.

## Firmware maintenance failed

- Keep the robot powered during all writes.
- Record the failed stage, target version, and client logs.
- If the firmware bundle did not finish downloading or verification failed, check the network and download it again. The robot has not been modified at this stage.
- If USB flashing has started, reconnect USB and run the complete flash again.
- Use [USB wired recovery](../guide/firmware-version-management.md#usb-wired-recovery) when the robot cannot boot.

## Before reporting an issue

Include the client and firmware versions, operating system, connection method, network environment, reproduction steps, expected and actual results, and relevant redacted logs.

Ask through [GitHub Issues](https://github.com/orulink-ai/WatcheRobot-public/issues) or the [WeChat community](../products/watcherobot/community.md). Do not publish Wi-Fi passwords, pairing codes, or service credentials.
