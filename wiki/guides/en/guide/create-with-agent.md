---
pageClass: agent-onboarding
---

# Create an app with an AI agent

Copy one prompt to your AI and let it help you build your first robot app.

::: tip Before you start
- Windows / macOS and Python 3.10–3.12; ask your AI to help prepare anything missing.
- A tool that can read URLs, edit files and run commands, such as Codex, Claude Code or TRAE; allow project dependency downloads.
- You can generate code without a robot; connect one later to test real effects.
:::

## 01 · Copy to your AI

<SkillPrompt english />

::: details What if the download fails?
[Download the complete Skill ZIP](/downloads/watche-robot-app.zip), extract it and give your AI the local `watche-robot-app` folder. Ask it to read `SKILL.md` and retain all templates, references and scripts.

You may also <a href="/downloads/watche-robot-app/SKILL.md" download>download raw SKILL.md</a>, but that file alone cannot replace the complete bundle. If your AI cannot reach the link, use the local folder. References in the bundle are currently in Chinese; the AI needs to read or translate them.
:::

## 02 · Let your AI deliver

Your AI will tell you where the project is, how to start it and what has not been verified on hardware. Have it complete code and local checks first, then help connect and run the robot after your confirmation.

## 03 · Connect and check the real experience

::: warning Motion safety first
Leave safe clearance. A successful command does not prove physical position, and a timeout does not prove motion has stopped. Current servos have no physical position feedback.
:::

See the [Python SDK quick start](/en/docs/sdk) for connection. Check only what your app uses:

- **Photos:** inspect an actual photo captured during this test.
- **Audio:** listen to the expected content; errors should be clear.
- **Lights / motion:** observe the intended effect and safe range; an ACK is not a measured position.
- **Repeated triggers:** no conflicting stacked tasks; failures allow explicit retries.
- **Stop:** the app exits and releases its own resources without affecting other apps. Ask the AI to report cleanup it cannot confirm.

## Need more capabilities?

[Capabilities and APIs](/en/products/watcherobot/capabilities) · [Voice applications](/en/products/watcherobot/voice) · [Community and support](/en/products/watcherobot/community)

This guide creates computer-side Python apps. For device firmware, use the [C SDK preview](/en/docs/embedded-sdk).
