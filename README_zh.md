[English](README.md) | 简体中文

# WatcheRobot

WatcheRobot 桌面机器人公开资料仓库，提供硬件资料、机械模型、Python SDK、烧录工具、使用文档和 Release 资产说明。

在线文档：**[wiki.orulink.ai](https://wiki.orulink.ai/)**。网站以本仓库文档为内容源，随正式 Release 更新；[维护与发布说明](wiki/README.md)。

## 获取机器人

想直接体验整机，省去采购零件和自行组装的过程？**[前往京东预购 WatcheRobot](https://item.jd.com/100326823811.html)**（中国大陆购买渠道）。

海外购买平台即将上线，上线后会在这里补充链接。预购规则及预计发货时间以京东商品页说明为准。

想自己动手制作？请查看[复现指引](wiki/guides/guide/reproduction.md)，获取硬件资料、装配和烧录步骤。

## 项目预览

![WatcheRobot 桌面机器人](docs/images/watcher-robot-render.png)

WatcheRobot 面向桌面陪伴、交互展示和开发者实验。整机由 Watcher 头部、ESP32-S3、STM32 身体主板、舵机、结构件和 SD 卡资源组成，可通过桌面端、手机 App 或 Python SDK 使用。

## 快速开始

适用于已有可用整机的用户，无需先克隆仓库、准备烧录工具或重新烧录。

1. **开机并安装客户端**：按[设备使用指南](wiki/guides/products/watcherobot/device-guide.md#install-app)下载适合电脑或手机的安装包。
2. **配置网络**：用手机 App 扫描设备二维码，完成[Wi-Fi 配网](wiki/guides/products/watcherobot/device-guide.md#wifi)。
3. **连接机器人**：电脑或 SDK 使用 Desktop Link；手机控制使用 Phone Control，见[连接说明](wiki/guides/guide/getting-started.md)。
4. **开始使用**：按[首次运行检查](docs/action-test_zh.md)验证表情、灯效和安全范围内的动作。

开发应用请看 [SDK 快速开始](docs/sdk_zh.md)，完整操作请看[设备使用指南](wiki/guides/products/watcherobot/device-guide.md)。

## 复现指引

想自己动手制作？请按[复现指引](wiki/guides/guide/reproduction.md)获取硬件资料和物料清单，完成装配、配套固件烧录与 SD 卡资源准备，再回到快速开始连接使用。

## 开发自己的嵌入式固件（开发预览，尚未公开发布）

除了通过 [Python SDK](docs/sdk_zh.md)编写上位机应用，我们也在准备面向
ESP32-S3 的独立硬件 SDK：开发者拥有自己的 `app_main`、UI 和业务逻辑，
通过统一接口使用身体运动、灯光、身体触摸，以及摄像头、麦克风、扬声器、
屏幕和屏幕触摸，不需要启动官方应用、Python SDK、Daemon 或服务端。

首版目标是提供身体、头部和整机三个可独立构建的 example；STM32 和 Himax
使用经过验证的配对固件，不要求开发者修改它们。详细边界和发布门禁见
[独立嵌入式开发说明](docs/embedded-sdk_zh.md)。

源码与三个示例位于 [firmware/sdk/](firmware/sdk/README_zh.md)，目前为开发预览，
尚未完成全部实机验收与固定版本发布。现有 Release 和烧录工具用于官方固件，
不能当作自定义固件的日常烧录入口。官方应用及 STM32/Himax 实现不在 SDK 开源范围内。

## 项目文档

- [烧录指南](docs/flashing_zh.md)：环境、接线、固件烧录、SD 卡和开机检查。
- [固件入口](firmware/README_zh.md)：各硬件对应的下载包和烧录入口。
- [SD 卡资源](docs/sd-card-assets_zh.md)和[首次运行检查](docs/action-test_zh.md)。
- [SDK 使用指南](docs/sdk_zh.md)：安装、配对和运行 Application。
- [独立嵌入式开发说明](docs/embedded-sdk_zh.md)：硬件 SDK 目标、示例与当前发布状态。
- [硬件资料](hardware/README_zh.md)：PCB 和机械文件入口。
- [设备使用指南](wiki/guides/products/watcherobot/device-guide.md)：开关机、扫码配网、连接电脑和手机。
- [机器人原子能力](wiki/guides/products/watcherobot/capabilities.md)、[快速创建语音对话](wiki/guides/products/watcherobot/voice.md)和[微信社区](wiki/guides/products/watcherobot/community.md)。
- [版本来源](docs/versions_zh.md)、[配套关系](docs/compatibility_zh.md)、[发布规则](docs/release-process_zh.md)和[仓库规则](docs/governance_zh.md)。
- [贡献指南](CONTRIBUTING_zh.md)和[安全策略](SECURITY_zh.md)。

## 开源范围

- 本仓库公开硬件生产文件、机械模型、Python SDK、烧录工具和说明文档。
- Python SDK 源码通过 `python-sdk/` 子模块提供。
- 官方 ESP32-S3 应用、Himax、STM32、App、服务端和桌面端实现不在本仓库公开；独立嵌入式硬件 SDK 以开发预览形式提供。
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
