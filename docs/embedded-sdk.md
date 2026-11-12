English | [简体中文](embedded-sdk_zh.md)

# Embedded C SDK (Development Preview)

The C SDK is now maintained in the public **[WatcheRobot_c_sdk](https://github.com/orulink-ai/WatcheRobot_c_sdk)** repository. Use that repository for current source, examples, dependencies and instructions. No private checkout is required.

## Choose your development route

| What you want to do | Start here |
| --- | --- |
| Write computer-side applications controlling existing robot firmware | [Python SDK](sdk.md) |
| Write C applications on ESP32-S3 using the screen, audio, camera, lights or motion | [C SDK overview](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/README.md) |

The routes are independent: the C SDK does not require installing the Python SDK or Runtime/Daemon first.

## Start from the standalone repository

1. Follow [your first C application](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/docs/getting-started.md) to install the required ESP-IDF, clone the complete standalone repository and run `hello_display`, which does not start body movement. Build and flash commands run from the **`WatcheRobot_c_sdk` repository root**, not this public repository.
2. Find APIs in the [feature guide](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/docs/features.md) and choose a project from the [examples](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/examples/README.md).
3. For a guided hardware check, use the [hardware-check example](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/examples/hardware_check/README.md).

## Before flashing or moving

- A full first flash replaces the application and partition table and may overwrite configuration. Check [hardware, paired firmware and backup requirements](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/docs/hardware-compatibility.md) and obtain usable recovery images first. Do not flash before confirming compatible versions.
- Restore official functionality using the [complete recovery procedure](flashing.md#full), not by writing only an old application.
- Servos cannot report their actual angle. Command coordinates and ACK/DONE do not prove arrival or a physical stop. Clear the full travel range, follow the [body API safety instructions](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/docs/api/body.md) and observe actual movement.
- The C SDK remains a **development preview**. A separate repository does not establish a stable release, complete firmware pairing acceptance or hardware safety certification.

## Fetch the SDK through this public repository

The original `firmware/` directory has been removed. Like `python-sdk`, the root-level **`c-sdk` Git submodule** points to the standalone repository. Old Wiki links redirect to the current guide; old snapshots remain in Git history.

From the public repository root, fetch the pinned C SDK version:

```sh
git submodule update --init c-sdk
cd c-sdk
```

Follow its getting-started guide from inside `c-sdk`. To fetch the Python SDK as well, run `git submodule update --init --recursive` from the public repository root. Submodules pin specific commits; they do not automatically track upstream main. Clone the standalone repository directly to develop against its latest revision. File SDK issues and contribute there.
