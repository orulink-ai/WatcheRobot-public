---
pageClass: agent-onboarding
---

# 用 AI Agent 创建应用

复制一段话给你的 AI，它会帮你做出第一个机器人程序。

::: tip 开始前确认
- Windows / macOS，Python 3.10–3.12；没有就让 AI 帮你准备。
- 可读取链接、修改文件、运行命令的 AI 工具，如 Codex、Claude Code、TRAE；允许联网安装本项目依赖。
- 没有机器人也能先生成代码，连接后再验证真实效果。
:::

## 01 · 复制给你的 AI

<SkillPrompt />

::: details 下载失败怎么办
[下载完整 Skill ZIP](/downloads/watche-robot-app.zip)，解压后把本机 `watche-robot-app` 文件夹交给 AI，让它读取其中的 `SKILL.md`，保留配套模板、参考资料和脚本。

也可以<a href="/downloads/watche-robot-app/SKILL.md" download>下载 SKILL.md 原文</a>查看说明，但单个文件不能替代完整 ZIP。若 AI 无法访问链接，使用下载后的本地文件夹即可。
:::

## 02 · 等 AI 交付

AI 会告诉你：项目在哪、怎么启动、哪些还没在真机上验证。让它先完成代码和本地检查，再在你确认后协助连接并运行机器人。

## 03 · 连上机器人，逐项看效果

::: warning 先确认运动安全
留出运动空间；命令成功不等于实际到位，超时也不代表停止。当前舵机没有物理位置反馈。
:::

连接方式见 [Python SDK 快速开始](/docs/sdk)。只验收本次应用用到的功能：

- **拍照**：看到本次真实拍摄的照片。
- **声音**：实际听到预期内容，失败时有明确提示。
- **灯光／动作**：现场观察符合预期且在安全范围内，不以 ACK 代替实测。
- **重复触发**：忙碌时不叠加冲突任务，失败后可以明确重试。
- **停止**：本应用退出并释放资源，其他应用不受影响；无法确认的清理让 AI 说明。

## 需要更多能力？

[全部功能与 API](/products/watcherobot/capabilities) · [语音应用](/products/watcherobot/voice) · [社区与支持](/products/watcherobot/community)

本页创建的是电脑端 Python 应用；设备端固件开发请走 [C SDK（开发预览）](/docs/embedded-sdk)。
