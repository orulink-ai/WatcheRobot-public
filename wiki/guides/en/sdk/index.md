English | [简体中文](../../sdk/index.md)

# SDK and Applications

The Python SDK runs on your computer and includes the Runtime/Daemon. It is separate from the [embedded hardware SDK](../../../../docs/embedded-sdk.md) compiled onto ESP32.

## Where to start

Follow the [Python SDK quick start](../../../../docs/sdk.md) to install, pair, create, and run an application. Then explore:

- [Robot capabilities](../products/watcherobot/capabilities.md): interfaces and limitations.
- [Voice conversations](../products/watcherobot/voice.md): a customizable ASR → LLM → TTS application.

## Three roles

| Role | Responsibility |
| --- | --- |
| Watcher Desktop | User interface, local runtime supervision, and application entry points |
| SDK Daemon | Device connections, pairing, application lifecycle, and message channels |
| Application | Business logic, model services, and motion orchestration through `app.robot` and `app.desktop` |

Applications receive injected channels through `ApplicationContext.from_environment()` and start with `watcherobot app run`. Do not create a separate connection that bypasses the Daemon.

While an Application runs, Desktop business messages go to it to decide how to respond. Only when no Application runs does the Daemon transparently forward messages. Custom applications must handle or safely ignore unsupported desktop messages.

## Versions and verification

These Python guides were checked against 0.1.12. The installed package, Desktop-bundled runtime, source submodule, and firmware have separate versions; record each during troubleshooting.

Passing `app check`, receiving an ACK, completing a Job, and observing the physical result are different verification levels. Validate your actual device and application combination before use.
