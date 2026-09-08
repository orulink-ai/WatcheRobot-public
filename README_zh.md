[English](README.md) | 简体中文

# WatcheRobot

## 项目预览

![WatcheRobot 桌面机器人](docs/images/watcher-robot-render.png)

WatcheRobot 支持表情、动作和音视频交互，可以通过桌面端、手机 App 或 Python SDK 使用。本仓库提供硬件资料、机械模型、SDK 和使用说明；固件及安装包从 [Releases](https://github.com/orulink-ai/WatcheRobot-public/releases) 下载。

## 快速开始

### 1. 获取仓库

```bash
git clone --recurse-submodules https://github.com/orulink-ai/WatcheRobot-public.git
cd WatcheRobot-public
```

已克隆但 SDK 文件夹为空时，运行 `git submodule update --init --recursive`。

### 2. 准备环境和材料

材料清单和组装步骤见[立创开源硬件项目](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl)。

整机主要用到 Watcher 头部、STM32 身体主板、舵机、结构件、电源和 SD 卡。完整清单以立创项目为准。

| 准备什么 | 用来做什么 |
| --- | --- |
| ST-LINK V2 | 烧录身体主板上的 STM32 |
| USB 数据线 | 烧录 Watcher 头部的 Himax 和 ESP32-S3 |
| Conda 专用环境 | 脚本自动准备 Python 依赖和烧录工具，不使用原有 Python 环境 |
| SD 卡、读卡器 | 写入表情和动作资源 |
| 电脑或手机 | 安装桌面端、App，或运行 SDK |

环境准备和可复制的命令统一放在烧录指南中。

### 3. 烧录并准备开机

按[烧录指南](docs/flashing_zh.md)依次完成身体主板、头部和 SD 卡准备，再上电。指南包含接线图和可直接复制的命令。

### 4. 连接并使用机器人

从 [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest) 选择需要的客户端：

| 使用方式 | 下载什么 |
| --- | --- |
| Windows 桌面端 | x64 安装 `.exe` |
| macOS 桌面端 | Apple Silicon `.dmg` |
| Android App | `.apk` |
| iOS App | [TestFlight](https://testflight.apple.com/join/XFCFsm5M) |
| Python SDK | `watcherobot` 的 `.whl`，按 [SDK 使用指南](docs/sdk_zh.md)安装和运行 |

网络配置见[设备网络配置说明](docs/manuals/device-network-setup.pdf)。连接后，先尝试一个表情和一个动作，确认屏幕、灯效和运动正常。

## 文档与资料

- [烧录指南](docs/flashing_zh.md)：接线、创建环境、烧录、SD 卡和开机检查。
- [SDK 使用指南](docs/sdk_zh.md)：安装、配对和运行 Python 脚本。
- [硬件资料](hardware/README_zh.md)：PCB 和机械文件。
- [完整使用说明书（中文 PDF）](docs/manuals/WatcheRobot-user-manual-20260724.pdf)。

固件源码不在本仓库公开；Python SDK 通过 `python-sdk/` 子模块提供。其他发布范围见[仓库说明](docs/governance_zh.md)。

## 许可证

本仓库使用 [GPL-3.0](LICENSE)。子项目和第三方组件以各自的许可证为准。
