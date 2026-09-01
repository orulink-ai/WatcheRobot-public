<div align="center">

<p><a href="README.md">English</a> | <strong>简体中文</strong></p>

<img src="docs/images/watcher-robot-render.png" alt="WatcheRobot 渲染图" width="720">

<p>WatcheRobot 桌面机器人开源资料包，面向硬件复现、结构参考、SDK 集成、固件烧录和设备使用验证。</p>

<p>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL--3.0-blue.svg" alt="License: GPL-3.0"></a>
  <img src="https://img.shields.io/badge/Package-0.1.0-brightgreen" alt="Package 0.1.0">
  <img src="https://img.shields.io/badge/Firmware-Release%20Assets-green" alt="Firmware release assets">
  <img src="https://img.shields.io/badge/Hardware-Gerber%20%7C%20BOM%20%7C%20CPL-orange" alt="Hardware: Gerber, BOM, CPL">
  <img src="https://img.shields.io/badge/SDK-Python-blue" alt="Python SDK">
</p>

</div>

---

## 项目概览

WatcheRobot 是一个桌面交互机器人项目，整机由 SenseCAP Watcher、ESP32-S3、STM32F103 协处理器、自研 PCB、机械结构件和 SD 卡行为资源组成。这个仓库用于公开复现和集成所需的资料，包括硬件生产文件、机械模型、Python SDK、烧录工具、设备说明书和配套 Release 资产说明。

当前开源边界已经调整：本仓库不公开 ESP32-S3 和 STM32F103 的嵌入式固件源码。固件以同一套版本的预编译 Release 资产提供，用户可以按文档完成烧录、准备 SD 卡资源并验证设备启动。

通过 Python SDK，开发者可以在主机侧控制 WatcheRobot 的 ESP32-S3，使用摄像头、麦克风、喇叭、内置表情和动效等能力。更多接口和示例请查看 [Python SDK 说明](python-sdk/README.zh-CN.md)。

## 快速开始

这部分用于帮助你从零拿到仓库、准备工具，并完成一次最小可验证的启动流程。

### 1. 获取仓库

```bash
git clone https://github.com/orulink-ai/WatcheRobot.git
cd WatcheRobot
```

如果你计划提交改动，建议先 Fork 仓库，再从自己的 Fork 创建分支。

### 2. 准备工具

| 用途 | 工具 |
| --- | --- |
| 基础环境 | Git、Python 3.11+ |
| ESP32-S3 烧录 | USB 串口驱动、仓库内烧录工具或 Release 中的 AI 烧录 Skill |
| STM32F103 烧录 | ST-LINK 或兼容 SWD 工具，配合同版本 Release 固件包 |
| SD 卡资源 | FAT32 格式 SD 卡、同版本 SD 卡资源包 |
| 硬件验证 | 串口工具、万用表或基础硬件调试工具 |

### 3. 获取固件和资源

推荐优先从 [GitHub Releases](https://github.com/orulink-ai/WatcheRobot/releases) 下载同一个套装中的资源。首次复现至少需要 ESP32-S3 固件、STM32F103 固件和 SD 卡资源，不要混用不同版本的资产；Release 中也会提供 AI 烧录 Skill 压缩包，可交给 AI 助手读取后协助烧录。

当前 `watche-v0.1.1` 套装的资源清单见 [下载说明](docs/downloads.md)。

### 4. 烧录固件并准备 SD 卡

拿到 Release 资源后，需要先完成 STM32F103 固件烧录、ESP32-S3 固件烧录和 SD 卡资源写入，再进行启动验证。具体步骤请参考 [Firmware 说明：烧录和资源](firmware/README_zh.md#烧录和资源)。

如果希望由 AI 助手协助烧录，可以下载 Release 中的 `WatcheRobot-Flashing-Skill-v0.1.1.zip`，或直接让 AI 阅读 [WatcheRobot 固件烧录 Skill](tools/flashing/README_zh.md)。这个 Skill 会引导 AI 选择同版本资源、识别串口、执行烧录并检查启动日志。

### 5. 验证第一次启动

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

python-sdk/
  README.md       Python SDK 源码、示例和测试

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
  release-process.md      Release 流程和资产规则
  governance.md           仓库边界说明

tools/
  ...             烧录、发布和资源辅助工具
```

## 项目文档

- [固件和烧录说明](firmware/README_zh.md)
- [刷写工具说明](docs/flashing.md)
- [SD 卡行为资源](docs/sd-card-assets.md)
- [行为资源 checklist](docs/behavior-flash-skill.md)
- [首次启动验证](docs/action-test.md)
- 如何配置设备网络，请查看 [设备网络配置说明](docs/manuals/device-network-setup.pdf)。
- 完整的使用说明，请查看 [WatcheRobot 使用说明书](docs/manuals/WatcheRobot-user-manual-20260724.pdf)。
- [SDK 和公开接口边界](docs/sdk.md)
- [Python SDK](python-sdk/README.zh-CN.md)
- [版本追踪](docs/versions.md)
- [贡献指南](CONTRIBUTING_zh.md)
- [安全策略](SECURITY.md)

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
- 配套 manifest 和校验文件

Release 资产类型和当前发布状态见 [docs/downloads.md](docs/downloads.md)。

## 技术栈

| 领域 | 主要内容 |
| --- | --- |
| Python SDK | Python 包、示例、主机侧测试 |
| 硬件 | EasyEDA Pro, Gerber, BOM, CPL, STEP |
| 固件资产 | ESP32-S3、STM32F103 预编译 Release 包 |
| 烧录工具 | Python、Windows 辅助脚本、AI 烧录 Skill |

## 许可证

本仓库使用 [GPL-3.0](LICENSE) 许可证。若子项目或第三方组件包含独立许可证声明，以其自身声明为准。
