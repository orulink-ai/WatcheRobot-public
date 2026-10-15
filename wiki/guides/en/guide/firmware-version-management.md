# Firmware version management

Firmware switching, Factory Reset, and USB recovery solve different problems. Factory Reset does not change the firmware version. Use Watcher Desktop and a USB connection to upgrade or downgrade firmware.

## Switch firmware versions in the client

Watcher Desktop downloads the selected firmware bundle to the computer and verifies it before writing anything to the robot over USB.

Before you start:

- Connect the robot to the computer over USB and keep it on stable power.
- Select the correct robot, serial port, and firmware version in the client.
- Close serial monitors or other programs that are using the same port.

After starting, wait for the client to finish downloading and verifying the bundle, flashing it over USB, and restarting the robot. Do not disconnect USB or turn off the robot before the client reports completion.

::: info A download failure does not modify the robot
If downloading or verifying the bundle fails on the computer, USB flashing has not started. The robot continues to run its previous firmware version. Check the network connection and download the bundle again.
:::

## Factory Reset

Use Factory Reset to remove network, pairing, and service connection information from the robot, or before giving the robot to another person.

::: warning The firmware version does not change
Factory Reset does not install, upgrade, or downgrade firmware. After restarting, the robot runs the same firmware version it had before the reset.
:::

Factory Reset clears or resets:

- Saved Wi-Fi names, passwords, and connection settings.
- Screen brightness and system volume, which return to the firmware defaults.
- Client pairing information and the pairing secret. The robot must be paired with the client again.
- Locally stored voice-service connection addresses, accounts, passwords, and access tokens.
- The locally stored device identifier and internal runtime state. A new device identifier is generated during a later startup.

Factory Reset does not delete:

- The currently installed firmware.
- Photos, recordings, or other files on the SD card.
- Applications managed on the computer by Watcher Desktop and the SDK Daemon.

### Steps

1. Open **Settings** on the robot.
2. Select **Factory Reset**.
3. Swipe as instructed on the screen to confirm.
4. Wait for the robot to restart. Do not turn off the power before it finishes.

### Reconnect after the reset

After the robot restarts, connect it to Wi-Fi and pair it again through Watcher Desktop. If you use a voice service, follow the client instructions to configure or activate it again. You can readjust screen brightness and system volume in the robot settings.

## If firmware installation is interrupted

Factory Reset cannot repair firmware that was not written completely. The correct action depends on when the interruption occurred:

| Interrupted stage | Was the robot modified? | What to do |
| --- | --- | --- |
| The client is downloading or verifying the firmware bundle | No | Check the network connection and download it again |
| USB flashing has started | Some data may already have been written | Keep or reconnect USB and run the complete flash again |
| The robot does not start after flashing | The firmware may be incomplete | Flash official firmware again in the client; if necessary, put the robot into USB download mode and retry |

USB flashing writes the bootloader, partition table, and main firmware. Disconnecting USB or losing power during this process may prevent the robot from starting. The robot is not guaranteed to restore its previous version automatically.

## USB wired recovery

If the robot cannot start normally or the client cannot connect to it, keep it connected over USB and use the client to flash official firmware for that robot again. Do not erase the whole device unless the official instructions explicitly require it.

## Choose the right action

| Your goal | Feature to use | Does the firmware version change? |
| --- | --- | --- |
| Upgrade or downgrade | Client USB firmware flashing | Yes |
| Clear network, pairing, and local service configuration | Factory Reset | No |
| Firmware bundle download failed | Download it again in the client | No |
| USB flashing was interrupted or the robot cannot start | Run USB firmware flashing again | Determined by the firmware version you select |
