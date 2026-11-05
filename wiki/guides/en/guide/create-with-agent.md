# Create an app with an AI agent

Describe your idea and let an AI agent help turn it into a WatcheRobot application.

We recommend the **WatcheRobot App Skill**. It gives your agent a development workflow, complete application templates and robot capability references, so you can start with an idea rather than reading the entire SDK manual.

## 01 · Send this prompt to your agent

Open an AI coding tool that can read URLs, edit local files and run commands, such as Codex, Claude Code or TRAE. Send this prompt from your project workspace and replace the requirement with your own idea.

<SkillPrompt english />

[Download the complete Skill ZIP](/downloads/watche-robot-app.zip) · <a href="/downloads/watche-robot-app/SKILL.md" download>Download raw SKILL.md</a>

This is a prompt for your agent, **not a terminal installation command**. It supplies the complete Skill for this task; it does not promise persistent installation in every agent product. If downloads fail, download and extract the ZIP manually, give the agent the local `watche-robot-app` folder and ask it to read `SKILL.md`. Copying only that file omits the supporting scripts and templates. The bundle's reference material is currently in Chinese; an agent must be able to read it or translate it.

## 02 · Describe one small experience

Specify a trigger, the robot's behavior and an observable result. For example:

> Create a touch interaction app: touching the robot's back turns its light blue and plays an expression that actually exists on the device. Repeated touches must not stack tasks. Release the app's resources when it stops. Complete the code and local checks first; wait for my confirmation before running on a robot.

Other ideas include a web photo button, a timer or your own voice service. Have the agent inspect available expression, action and sound resources instead of guessing IDs. Cloud services need your own credentials; keep secrets out of source code and chat messages.

## 03 · Have the agent build and check it

The Skill includes one-shot, input-listener and web-button templates, plus references for motion, lighting, expressions, audio, camera, touch and RTC. The agent chooses the relevant template, prepares an isolated environment, checks the installed SDK interfaces, then develops and validates the app.

- **Requirements:** Python 3.10–3.12 (3.11 recommended), network access for SDK installation, and agent permission to use files and a terminal. If anything is missing, ask the agent to explain and help prepare it.
- **Deliverables:** a project folder, check results, the full launch command and usage instructions.
- **Runtime:** the application runs on your computer through the SDK Daemon using `watcherobot app run`. It is not firmware flashed onto the robot.

## 04 · Test it on a real robot

After local checks, connect your robot using the [Python SDK quick start](/en/docs/sdk). You can generate and check code without a device, but real hardware behavior remains unverified.

Before starting, have the agent confirm the target device and resource ownership; do not take over another running app. Leave safe clearance for motion tests. Current servos have no physical position feedback: a successful command is not a measured position.

Check resource cleanup after stopping. Inspect the actual photo and test sound and motion in person; successful checks or command acknowledgments are not proof of real-world effects.

## Need an interface reference?

[Capabilities and APIs](/en/products/watcherobot/capabilities) · [Voice applications](/en/products/watcherobot/voice) · [Community and support](/en/products/watcherobot/community)

This Skill targets computer-side Python apps. For device-side development, use the [C SDK preview](/en/docs/embedded-sdk).
