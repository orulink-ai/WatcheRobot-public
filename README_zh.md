[English](README.md) | 简体中文

# WatcheRobot

WatcheRobot 桌面机器人公开资料仓库，提供硬件资料、机械模型、Python SDK、烧录工具、使用文档和 Release 资产说明。

## 项目预览

![WatcheRobot 桌面机器人](docs/images/watcher-robot-render.png)

WatcheRobot 面向桌面陪伴、交互展示和开发者实验。整机由 Watcher 头部、ESP32-S3、STM32 身体主板、舵机、结构件和 SD 卡资源组成，可通过桌面端、手机 App 或 Python SDK 使用。

## 快速开始

### 1. 获取仓库

```bash
git clone --recurse-submodules https://github.com/orulink-ai/WatcheRobot-public.git
cd WatcheRobot-public
```

已克隆但 `python-sdk/` 为空时，运行：

```bash
git submodule update --init --recursive
```

### 2. 准备环境和材料

物料清单和装配步骤见[立创开源硬件项目](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl)。

| 准备内容 | 用途 |
| --- | --- |
| 已装配的 Watcher 头部和机器人身体 | 烧录并运行机器人 |
| ST-LINK V2 | 烧录身体主板上的 STM32 |
| USB 数据线 | 烧录头部的 Himax 和 ESP32-S3 |
| SD 卡和读卡器 | 写入表情、动作等资源 |
| Conda | 创建独立环境；其余依赖和烧录工具由脚本准备 |
| 电脑或手机 | 使用桌面端、App 或 SDK |

### 3. 准备机器人

打开[烧录指南](docs/flashing_zh.md)，按章节依次完成：

1. 烧录身体主板上的 STM32。
2. 烧录 Watcher 头部；脚本先写入 Himax，再写入 ESP32-S3。
3. 用读卡器把 SD 资源压缩包解压到 SD 卡根目录，并将卡插回头部。
4. 上电并完成启动检查。

### 4. 连接并使用

从 [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest) 下载所需客户端或 SDK：

| 使用方式 | 下载内容 |
| --- | --- |
| Windows 桌面端 | x64 `.exe` 安装包 |
| macOS 桌面端 | Apple Silicon `.dmg` 安装包 |
| Android App | `.apk` 安装包 |
| iOS App | [TestFlight](https://testflight.apple.com/join/XFCFsm5M) |
| Python SDK | `watcherobot` `.whl`，按 [SDK 使用指南](docs/sdk_zh.md)操作 |

网络配置见[设备网络配置说明](docs/manuals/device-network-setup.pdf)。连接后，可运行一个表情和一个动作检查屏幕、灯效和运动。

## 目录结构

```text
WatcheRobot-public/
├─ docs/          使用、烧录、SDK 和发布说明
├─ firmware/      固件与 SD 资源下载入口
├─ hardware/      PCB、BOM、Gerber 和机械模型
├─ python-sdk/    Python SDK 子模块
├─ skills/        Codex 烧录 Skill
├─ tools/         Windows、macOS 和 Linux 烧录脚本
└─ .github/       Issue、PR 和 CI 配置
```

## 项目文档

- [烧录指南](docs/flashing_zh.md)：环境、接线、固件烧录、SD 卡和开机检查。
- [固件入口](firmware/README_zh.md)：各硬件对应的下载包和烧录入口。
- [SD 卡资源](docs/sd-card-assets_zh.md)和[首次运行检查](docs/action-test_zh.md)。
- [SDK 使用指南](docs/sdk_zh.md)：安装、配对和运行 Python 脚本。
- [硬件资料](hardware/README_zh.md)：PCB 和机械文件入口。
- [完整使用说明书（中文 PDF）](docs/manuals/WatcheRobot-user-manual-20260724.pdf)。
- [版本来源](docs/versions_zh.md)、[配套关系](docs/compatibility_zh.md)、[发布规则](docs/release-process_zh.md)和[仓库规则](docs/governance_zh.md)。

## 开源范围

- 本仓库公开硬件生产文件、机械模型、Python SDK、烧录工具和说明文档。
- Python SDK 源码通过 `python-sdk/` 子模块提供。
- ESP32-S3、Himax、STM32、App、服务端和桌面端源码不在本仓库公开。
- 固件、客户端安装包和 SD 卡资源通过 GitHub Releases 分发。

## 技术栈

| 部分 | 技术或格式 |
| --- | --- |
| 头部 | Watcher、Himax、ESP32-S3 |
| 身体控制 | STM32F103、SWD / ST-LINK |
| SDK 与烧录工具 | Python、PowerShell、Bash |
| 硬件资料 | 原理图、PCB、BOM、CPL、Gerber、STEP |

## 许可证

本仓库使用 [GPL-3.0](LICENSE)。子项目和第三方组件以各自许可证为准。
