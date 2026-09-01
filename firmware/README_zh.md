<div align="center">

<p><a href="README.md">English</a> | <strong>简体中文</strong></p>

</div>

# Firmware

`firmware/` 是 WatcheRobot 固件烧录和资源准备入口。本仓库不公开 ESP32-S3 与 STM32F103 的固件源码；对应固件以 GitHub Release 中的预编译资产形式分发。

## 烧录和资源

首次复现建议从同一个 [GitHub Release](https://github.com/orulink-ai/WatcheRobot/releases) 套装中下载：

| 内容 | 文件 |
| --- | --- |
| ESP32-S3 固件 | `WatcheRobot-ESP32S3-v0.3.2.zip` |
| STM32F103 固件 | `WatcheRobot-STM32F103-v0.1.1.zip` |
| SD 卡资源 | `WatcheRobot-SDCard-Assets-v0.3.2.zip` |
| AI 烧录 Skill | `WatcheRobot-Flashing-Skill-v0.1.1.zip` |
| 版本和校验 | `WatcheRobot-Bundle-v0.1.1.manifest.json`、`SHA256SUMS.txt` |

不要混用不同 Release 中的固件和 SD 卡资源。完整资源清单见 [下载说明](../docs/downloads.md)。

### AI 辅助烧录

仓库提供了面向 AI 助手的烧录 Skill：[WatcheRobot 固件烧录 Skill](../tools/flashing/README_zh.md)。Release 中也会提供同名压缩包 `WatcheRobot-Flashing-Skill-v0.1.1.zip`。如果你使用 Codex 或其他 AI 编程助手，可以直接让它读取这个 README 或压缩包内的 README，并帮你完成 Release 资源选择、串口识别、ESP32-S3 烧录、SD 卡资源准备和启动日志检查。

你可以直接这样说：

```text
请阅读 tools/flashing/README_zh.md，帮我烧录 WatcheRobot。
使用最新 Release 中的 ESP32-S3 固件、STM32F103 固件和 SD 卡资源，识别当前串口，烧录后帮我查看启动日志。
```

### ESP32-S3 Release ZIP 烧录

```bash
python -m pip install -r tools/win_flasher/requirements.txt
python -m tools.win_flasher list-ports
python -m tools.win_flasher flash --zip .\WatcheRobot-ESP32S3-v0.3.2.zip --port COM7 --monitor
```

Windows 下也可以使用：

```powershell
tools\flash-release.cmd --zip .\WatcheRobot-ESP32S3-v0.3.2.zip --port COM7 --monitor
```

### STM32F103 固件烧录

STM32F103 烧录依赖你的物理调试器和台架配置。公开仓不提供 STM32 源码构建入口，使用 Release 中的 `WatcheRobot-STM32F103-v0.1.1.zip` 作为烧录输入。

最小硬件要求：

- MCU：`STM32F103C8Tx`
- 调试器：ST-LINK 或兼容 SWD 工具
- 本地调试串口：`USART1 @ 115200 8N1`
- ESP32 协处理器链路：`USART2 @ 921600 8N1`

### SD 卡资源

SD 卡资源目录结构和写入说明见 [SD 卡资源说明](../docs/sd-card-assets.md)。串口驱动、端口识别和平台差异见 [固件刷写说明](../docs/flashing.md)。

## Release

预编译固件、烧录包、SD 卡动画资源包等发布产物不直接提交到 Git 仓库，应作为 GitHub Release 资产上传。

Release 说明中需要标明兼容的 App、Server、Desktop、硬件和模型版本。
