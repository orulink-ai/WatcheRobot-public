# Create an app with an AI agent

Start with one idea, without reading the entire SDK manual first.

The WatcheRobot App Skill gives your agent workflows, templates and capability references. Allow **3–5 minutes** to read this page and describe your idea. Installation, development and hardware testing take additional time depending on your network and requirements.

::: tip Before you start
- **Environment:** Windows / macOS for running the app; Python 3.10–3.12, preferably 3.11. If missing, ask the agent to help prepare it without changing the system default.
- **Tools:** an AI coding tool that can read URLs, edit files and use a terminal, with permission to download project dependencies.
- **Hardware:** code generation and local checks do not need a robot. Photos, lights, audio and motion require a connected WatcheRobot for acceptance.
:::

## 01 · Describe your idea and copy the prompt

Open your workspace in Codex, Claude Code, TRAE or another capable tool. Select a template or write your own requirement, then send the generated prompt to your agent.

<SkillPrompt english />

[Download the complete Skill ZIP](/downloads/watche-robot-app.zip) · <a href="/downloads/watche-robot-app/SKILL.md" download>Download raw SKILL.md</a>

This is a natural-language prompt for your agent, not a terminal command.

::: details Download failed, or want a persistent Skill installation?
Download the ZIP manually, extract the complete bundle and give your agent the local `watche-robot-app` folder path. Ask it to read `SKILL.md`; keep all templates, references and scripts, not just this file.

Persistent installation depends on your tool; there is no universal installation command here. The bundle references are currently in Chinese, so the agent must read or translate them.

The prompt uses this page's current origin. LAN preview downloads require an agent with company-network access; cloud agents may need a manually transferred folder instead. Production download URLs must be checked after publication; preview availability does not mean production is already updated.
:::

## 02 · Check the deliverables

Have the agent create the project, inspect the installed SDK interfaces and finish local validation before controlling a robot. The app runs on your computer through the SDK Daemon; it is not device firmware.

This is the actual project generated with the bundle's `button` template, not a mock screenshot:

```text
wiki-skill-example-python311/
├─ .gitignore
├─ .venv/
├─ .watcherignore
├─ README.md
├─ app.json
├─ app.py
├─ app_support.py
├─ icon.svg
├─ photo_app.py
├─ sdk-capabilities.json
├─ sdk-requirements.json
└─ skill-setup.json
```

Excerpt from real local validation (Python 3.11, SDK 0.1.13):

```text
No broken requirements found.
Application validated
本地检查通过；未启动 Daemon，未验证设备。
```

The last line means “local checks passed; Daemon was not started and the device was not verified.” Success requires `state: ready` in `skill-setup.json`, a check exit code of 0 and an explicit hardware-test status. This Linux test covers generation and static checks only; the generated manifest supports Windows / macOS, not Linux managed execution.

::: details Run on the target computer (after connection and run authorization)
Windows PowerShell, from your actual project directory:

```powershell
& ./.venv/Scripts/watcherobot.exe app run .
```

macOS, from your actual project directory:

```sh
./.venv/bin/watcherobot app run .
```

The web-photo app should print a local web address. Open it and click the button to see a real photo from this test. No robot was connected for this documentation check, so there is no successful `app run` log to show. A missing URL or real photo needs diagnosis; it is not hardware acceptance.
:::


Require a **project location, complete launch command, check results and unverified items**. A completion message or screenshot alone is insufficient.

## 03 · Connect and test the real experience

Follow the [Python SDK quick start](/en/docs/sdk). Confirm the target device and running application; do not take over someone else's app. Configure service credentials locally, outside source code and chat.

::: warning Motion safety
Leave safe clearance. Current servos have no physical position feedback: a successful command does not establish measured position, and a timeout does not prove motion has stopped.
:::

Check only the capabilities used by your app:

- **Photo:** inspect a real photo captured during this test, not a placeholder.
- **Audio:** listen to the expected content; errors must be clear.
- **Lights / motion:** observe the intended effect and safe range; an ACK is not a measured position.
- **Repeated triggers:** avoid conflicting stacked tasks and support explicit retries after failure.
- **Stop:** confirm the app exits and releases its own resources without affecting other apps. Report cleanup that cannot be confirmed.

## Next steps

[Capabilities and APIs](/en/products/watcherobot/capabilities) · [Voice applications](/en/products/watcherobot/voice) · [Community and support](/en/products/watcherobot/community)

This guide targets computer-side Python applications. For device-side development, use the [C SDK preview](/en/docs/embedded-sdk).
