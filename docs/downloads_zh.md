<p><a href="downloads.md">English</a> | <strong>简体中文</strong></p>

# 下载说明

打开 [Releases](https://github.com/orulink-ai/WatcheRobot-public/releases)，选择标记为 Latest 的版本，在 Assets 中按需要下载：

| 用途 | 下载附件 |
| --- | --- |
| 身体主板 | 名称含 `STM32` 的 ZIP |
| Watcher 头部 | 名称含 `PTL-paired` 的 ZIP，已含固件、脚本和 Skill |
| SD 卡 | 名称含 `sd-resources` 的压缩包 |
| Windows 客户端 | x64 安装 `.exe` |
| macOS 客户端 | Apple Silicon `.dmg` |
| Android | `.apk` |
| iOS | [TestFlight](https://testflight.apple.com/join/XFCFsm5M) |
| Python SDK | `watcherobot` 的 `.whl` |

看 Release 页面标注的版本即可；校验文件和 manifest 不需要普通用户单独下载。不要把 GitHub 自动生成的 Source code ZIP 当作烧录包。

第一次准备机器人，按[组装与烧录指引](flashing_zh.md)操作，其中包含创建 Python 环境和运行脚本的完整命令。使用 SDK 时看 [SDK 安装与运行](sdk_zh.md)。
