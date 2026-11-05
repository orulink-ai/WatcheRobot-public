# 用 AI Agent 创建应用

从一句需求开始，不必先读完整本 SDK 手册。

用 WatcheRobot App Skill 把开发流程、模板和能力说明交给 Agent。阅读并填写本页约需 **3–5 分钟**；安装、生成程序和实机测试耗时取决于网络与需求，不包含在这个估计中。

::: tip 开始前确认
- **环境**：运行电脑使用 Windows／macOS；Python 3.10–3.12，推荐 3.11；缺少时让 Agent 先协助准备，不改系统默认 Python。
- **工具**：使用可读取链接、编辑文件和运行终端命令的 AI 编程工具，并允许它为本项目联网安装依赖。
- **设备**：没有机器人也能先生成并检查代码；验证灯光、照片、声音或动作时，需要已连接的 WatcheRobot。
:::

## 01 · 填写需求，复制给 Agent

在 Codex、Claude Code、TRAE 等工具中打开你的工作区。选一个模板或写下自己的需求，再复制生成的提示词发送给 Agent。

<SkillPrompt />

[下载完整 Skill ZIP](/downloads/watche-robot-app.zip) · <a href="/downloads/watche-robot-app/SKILL.md" download>下载 SKILL.md 原文</a>

这是发给 Agent 的自然语言提示词，不是终端命令。

::: details 下载失败，或想安装为工具的长期 Skill？
手动下载上面的 ZIP，完整解压，把本机 `watche-robot-app` 文件夹路径交给 Agent，让它读取 `SKILL.md`。不要只复制单个文件，模板、参考资料和脚本也需要保留。

是否安装到工具的长期技能库，按所用工具的说明操作；这里不提供通用于所有 Agent 的安装命令。

提示词中的下载地址取自当前页面站点。局域网预览地址只适合能够访问公司网络的 Agent；远程云 Agent 无法访问时，请改用本地文件。正式站点下载地址需在发布后验收，本次预览不代表它已经上线。
:::

## 02 · 看 Agent 是否交付完整

先让 Agent 创建项目、核对实际安装的 SDK 接口并完成本地检查，暂不控制机器人。程序在电脑上通过 SDK Daemon 运行，不是刷进机器人的固件。

下面是本次用 ZIP 中的 `button` 模板实际生成的项目（不是示意截图）：

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

实际本地检查输出节选（Python 3.11、SDK 0.1.13）：

```text
No broken requirements found.
Application validated
本地检查通过；未启动 Daemon，未验证设备。
```

**如何判断成功**：准备结果 `skill-setup.json` 中 `state` 为 `ready`，检查命令退出码为 0，且 Agent 明确报告设备测试状态。本例在 Linux 仅完成生成和静态检查；生成清单支持 Windows / macOS，不代表 Linux 托管运行受支持。

::: details 在目标电脑上运行（需先确认连接与运行授权）
Windows PowerShell：在实际项目目录打开终端，执行：

```powershell
& ./.venv/Scripts/watcherobot.exe app run .
```

macOS：在实际项目目录打开终端，执行：

```sh
./.venv/bin/watcherobot app run .
```

网页拍照模板运行后，应显示本机网页地址；打开后点击按钮，应看到本次真实拍摄的照片。本次未连接机器人，没有可展示的 `app run` 成功日志。若没有网页地址或实际照片，应交给 Agent 排查，不算实机验收通过。
:::


交付时应提供 **项目位置、完整启动命令、检查结果和未验证事项**。成功截图或一段“完成了”的回答不能代替这些结果。

## 03 · 连接机器人，逐项验收

连接方法见 [Python SDK 快速开始](/docs/sdk)。确认目标设备和正在运行的应用，不接管他人的程序；云服务凭据在本机配置，不写进源码和聊天记录。

::: warning 有运动时先确认安全
留出运动空间。当前舵机没有物理位置反馈：命令成功不代表实测到位；超时也不代表已经停止。
:::

只检查本次应用用到的项目：

- **拍照**：看到本次拍摄的真实照片，而非占位图。
- **声音**：实际听到预期内容；错误时有明确提示。
- **灯光／动作**：现场观察效果和安全范围，不以 ACK 当作实测位置。
- **重复触发**：忙碌时不叠加冲突任务，失败后能明确重试。
- **停止应用**：确认本应用已退出并释放资源，其他应用不受影响；无法确认的清理需说明。

## 接下来

[全部功能与 API](/products/watcherobot/capabilities) · [语音应用](/products/watcherobot/voice) · [社区与支持](/products/watcherobot/community)

本页用于电脑端 Python 应用；设备端开发请走 [C SDK（开发预览）](/docs/embedded-sdk)。
