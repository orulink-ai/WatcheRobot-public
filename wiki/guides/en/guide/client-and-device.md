# Client and device

Watcher Desktop is the user-facing control surface. It supervises the SDK Daemon instead of maintaining a second device-connection implementation.

```text
Watcher Desktop
    ↓
SDK Daemon
    ├─↔ WatcheRobot device
    └─↔ selected Application
```

## Connection states

| State                   | Meaning                                                   | Recommended action                              |
| ----------------------- | --------------------------------------------------------- | ----------------------------------------------- |
| Device not found        | No available transport has discovered the robot           | Check power, network, and the local firewall    |
| Waiting for pairing     | The robot was discovered but is not trusted yet           | Read the code on the robot and complete pairing |
| Paired, disconnected    | A pairing exists but the current link is unavailable      | Put the computer and robot on the same network  |
| Connected               | The Daemon owns an active device link                     | Start a compatible Application                  |
| Application not running | The robot is online but no business Application is active | Select and start an Application in the client   |

## When to provision Wi-Fi again

Provision again only when the network or password changed, invalid credentials were stored, the robot moved to a different network, or device data was deliberately cleared.

A short disconnect does not imply corrupt credentials. Allow automatic reconnection before resetting network state.

## Message routing while an Application runs

Desktop business messages enter the selected Application first. The Application decides how to interact with the device. When no Application is running, the Daemon can transparently forward desktop and device traffic.

This boundary gives third-party Applications ownership of their protocol without allowing the desktop to open a second business connection to the robot.
