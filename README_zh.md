<div align="center">

<p><a href="README.md">English</a> | <strong>简体中文</strong></p>

<img src="docs/images/watcher-robot-render.png" alt="WatcheRobot 渲染图" width="720">

<p>WatcheRobot 桌面机器人公开资料仓库，提供硬件资料、机械模型、Python SDK、烧录工具、使用文档和 Release 资产说明。</p>

<p>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL--3.0-blue.svg" alt="License: GPL-3.0"></a>
  <img src="https://img.shields.io/badge/Release-latest-brightgreen" alt="最新 Release">
  <img src="https://img.shields.io/badge/Firmware-Release%20Assets-green" alt="Firmware release assets">
  <img src="https://img.shields.io/badge/Hardware-Gerber%20%7C%20BOM%20%7C%20CPL-orange" alt="Hardware: Gerber, BOM, CPL">
  <img src="https://img.shields.io/badge/SDK-Python-blue" alt="Python SDK">
</p>

</div>

---

## 项目概览

WatcheRobot 是一个面向桌面陪伴、交互展示和开发者实验的机器人套件，整机由 SenseCAP Watcher、ESP32-S3、STM32F103 协处理器、自研 PCB、机械结构件和 SD 卡行为资源组成。

本仓库用于发布 WatcheRobot 的公开资料和开发者接入能力。仓库中提供硬件生产文件、可编辑 PCB 工程、机械装配模型、Python SDK、烧录工具、网络配置说明、使用说明书和 Release 资产说明。ESP32-S3 与 STM32F103 固件以预编译 Release 资产提供，不在本仓库公开源码。

用户可以基于这些资料完成设备装配核对、固件烧录、网络配置、首次启动验证，以及通过 Python SDK 进行二次开发。SDK 支持在主机侧连接 WatcheRobot，调用摄像头、麦克风、喇叭、内置表情和动效等能力。更多接口和示例请查看 [Python SDK 说明](python-sdk/README.zh-CN.md)。

## 快速开始

这部分用于帮助你拿到公开资料、准备工具，并完成一次最小可验证的设备启动和 SDK 连接流程。

### 1. 获取仓库

```bash
git clone --recurse-submodules https://github.com/orulink-ai/WatcheRobot-public.git
cd WatcheRobot-public
```

如果此前克隆仓库时未包含子模块，请先执行 `git submodule update --init --recursive`，再使用 `python-sdk/`。

### 2. 准备环境

| 用途 | 所需环境 |
| --- | --- |
| 下载 | 浏览器 |
| ESP32-S3 + Himax PTL 配套烧录 | Python 3.10+、CH342 驱动、支持数据传输的 USB 线、配套包内 `requirements.txt` |
| STM32F103 烧录 | ST-LINK 或兼容 SWD 调试器、目标板供电、OpenOCD 或其他兼容烧录软件 |
| SD 卡准备 | FAT32 格式 SD 卡、读卡器、可解压 `.tar.gz` 的工具 |
| 桌面端 | Windows x64，或 Apple Silicon Mac |
| 移动端 | Android 用于安装 APK；iOS 需要 TestFlight |
| Python SDK | CPython 3.10、3.11 或 3.12，以及 `pip` |

### 3. 下载当前 Release

进入 [GitHub Releases](https://github.com/orulink-ai/WatcheRobot-public/releases)，打开标记为 **Latest** 的 Release，并且只使用该 Release 附带的资产。

| 组件 | Release 附件识别方式 |
| --- | --- |
| ESP32-S3 + Himax PTL 配套固件 | 文件名包含 `S3` 和 `PTL-paired` 的 ZIP |
| STM32F103 固件 | 文件名包含 `STM32` 的 ZIP |
| 设备 SD 卡资源 | 文件名包含 `sd-resources` 的 `.tar.gz` 压缩包 |
| Windows 桌面端 | x64 安装 `.exe` |
| macOS 桌面端 | Apple Silicon `.dmg` |
| Android App | `.apk` |
| iOS App | [通过 TestFlight 安装](https://testflight.apple.com/join/XFCFsm5M) |
| Python SDK | `watcherobot` `.whl` |

按 Release 页面标注的组件版本下载即可，不需要手动计算校验值。各平台应下载哪些文件，见[下载说明](docs/downloads_zh.md)。

### 4. 按顺序准备机器人

1. 准备板卡、元器件和机械结构件并完成装配。材料入口见[立创开源硬件项目](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl)和本仓库的[硬件资料](hardware/README_zh.md)。
2. 保持断电，检查电源极性、排线方向、连接器和可能的短路。
3. 找到机器人身体内部的反馈舵机 STM32 控制板，使用 ST-LINK/SWD 烧录 STM32F103。
4. 用 USB 数据线连接上方的 Watcher 头部，按[烧录指引](docs/flashing_zh.md)创建 Python 环境，复制命令依次烧录 Himax 和 ESP32-S3。
5. 从 Watcher 头部取出 SD 卡，用读卡器连接电脑。将卡格式化为 FAT32，把 `sd-resources` 压缩包内容解压到卡根目录，安全弹出后在机器人断电时插回。不能通过 Watcher 的 USB 或串口写入 SD 卡。
6. 重新连接已装配硬件，上电并执行首次启动验收。

完整命令和硬件要求见 [Firmware 说明：烧录和资源](firmware/README_zh.md#烧录和资源)。

使用 AI 辅助 PTL 配套烧录时，Skill 已包含在解压后的配套 ZIP 中。用 AI 编程助手打开解压目录，让它读取 `skills/watche-ptl-release-flash/SKILL.md` 即可，不需要另外下载单独 ESP32、Himax 或 Skill 包。

### 5. 安装客户端或 SDK

- Windows x64：运行当前 Release 中的 x64 安装 `.exe`。
- Apple Silicon macOS：打开当前 Release 中的 `.dmg` 并安装应用。
- Android：安装当前 Release 中的 `.apk`；如果系统提示，请允许当前来源安装应用。
- iOS：使用上方附件表中的 TestFlight 入口安装。
- Python：按 [SDK 安装与运行](docs/sdk_zh.md)创建环境、安装下载的 wheel 并连接机器人。

### 6. 验证第一次启动

上电后按 [首次启动验证清单](docs/action-test.md) 检查：

- ESP32-S3 能正常启动并输出日志
- SD 卡资源能被识别
- 舵机或执行器能完成一次基础动作
- 灯效或屏幕资源能正常显示
- 串口、BLE、WebSocket 或 SDK 入口能返回基础状态信息

完成以上步骤后，说明最小硬件、固件和资源链路已经跑通。

## 目录结构

```text
firmware/
  README.md       固件 Release 资产、烧录入口和资源说明

hardware/
  README.md       硬件资料地图和 BOM 说明
  pcb/            PCB 源文件、原理图、Layout、Gerber、BOM、CPL 和备用件模板
  3d-models/      机械模型导出文件
  assembly/       后续装配图片或装配文档

python-sdk/        包含 Python SDK 源码、示例和测试的 Git 子模块

docs/
  flashing.md             固件刷写和工具说明
  manuals/                设备网络配置和完整使用说明 PDF
  sd-card-assets.md       SD 卡行为资源说明
  behavior-flash-skill.md 行为资源操作 checklist
  action-test.md          首次启动验证清单
  sdk.md                  SDK 和公开接口边界
  versions.md             版本来源和追踪规则
  downloads.md            Release 资产说明
  compatibility.md        版本兼容矩阵
  governance.md           仓库边界说明

tools/
  ...             烧录、发布和资源辅助工具
```

## 项目文档

- [固件和烧录说明](firmware/README_zh.md)
- [刷写工具说明](docs/flashing_zh.md)
- [SD 卡行为资源](docs/sd-card-assets_zh.md)
- [行为资源检查清单](docs/behavior-flash-skill_zh.md)
- [首次启动验证](docs/action-test_zh.md)
- 如何配置设备网络，请查看 [设备网络配置说明](docs/manuals/device-network-setup.pdf)。
- 完整的使用说明，请查看 [WatcheRobot 使用说明书](docs/manuals/WatcheRobot-user-manual-20260724.pdf)。
- [SDK 和公开接口边界](docs/sdk_zh.md)
- [Python SDK](python-sdk/README.zh-CN.md)
- [版本追踪](docs/versions_zh.md)
- [安全策略](SECURITY_zh.md)

## 开源范围

本仓库公开：

- Python SDK 源码
- PCB 和机械结构公开资料
- 固件烧录、资源准备和发布辅助工具
- 设备说明书、网络配置说明和 Release 文档

以下内容不在本仓库公开源码：

- ESP32-S3 固件源码
- STM32F103 固件源码
- Android App 源码
- 服务端源码
- 桌面端源码

以下内容在可用时通过 GitHub Release 分发：

- ESP32-S3 预编译固件
- STM32F103 预编译固件
- SD 卡资源包
- Android App 安装包
- 桌面端安装包
- Python SDK wheel
- 配套 manifest 和校验文件

iOS App 通过 TestFlight 分发，不作为 Release 附件上传。

Release 资产类型和当前发布状态见[下载说明](docs/downloads_zh.md)。

## 技术栈

| 领域 | 主要内容 |
| --- | --- |
| Python SDK | Python 包、示例、主机侧测试 |
| 硬件 | EasyEDA Pro, Gerber, BOM, CPL, STEP |
| 固件资产 | ESP32-S3、STM32F103 预编译 Release 包 |
| 烧录工具 | Python、Windows 辅助脚本、AI 烧录 Skill |

## 许可证

本仓库使用 [GPL-3.0](LICENSE) 许可证。若子项目或第三方组件包含独立许可证声明，以其自身声明为准。
