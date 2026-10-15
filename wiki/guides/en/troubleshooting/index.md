# Troubleshooting

Start with the smallest failure domain. Do not clear user data or erase the entire flash as a first response.

## The client cannot find the robot

1. Confirm that the robot has completed startup and remains powered.
2. Put the computer and robot on the same local network.
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

## Firmware maintenance failed

- Keep the robot powered during all writes.
- Record the failed stage, target version, and client logs.
- If the firmware bundle did not finish downloading or verification failed, check the network and download it again. The robot has not been modified at this stage.
- If USB flashing has started, reconnect USB and run the complete flash again.
- Use [USB wired recovery](../guide/firmware-version-management.md#usb-wired-recovery) when the robot cannot boot.

## Before reporting an issue

Include the client and firmware versions, operating system, connection method, network environment, reproduction steps, expected and actual results, and relevant redacted logs.
