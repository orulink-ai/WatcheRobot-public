[English](../en/sdk/index.md) | 简体中文

# SDK 与 Application

Python SDK 是电脑上的机器人接口，包含 Runtime/Daemon；它与直接编译到 ESP32 的[嵌入式硬件 SDK](../../../docs/embedded-sdk_zh.md)是两条不同的开发路径。

## 从哪里开始

先按 [Python SDK 快速开始](../../../docs/sdk_zh.md)安装、配对、创建并运行应用。再选择：

- [原子能力](../products/watcherobot/capabilities.md)：设备接口与限制。
- [语音对话](../products/watcherobot/voice.md)：可修改的 ASR → LLM → TTS 应用。

## 三个角色

| 角色 | 负责什么 |
| --- | --- |
| Watcher Desktop | 用户界面，管理本地运行时和应用入口 |
| SDK Daemon | 设备连接、配对、Application 生命周期及消息通道 |
| Application | 业务逻辑、模型服务、动作编排，通过 `app.robot` 和 `app.desktop` 交互 |

Application 通过 `ApplicationContext.from_environment()` 接收注入通道，用 `watcherobot app run` 启动，不另建绕过 Daemon 的设备连接。

Application 运行时，Desktop 业务消息进入当前 Application，由它决定如何响应。不运行 Application 时，Daemon 才提供透明转发。自定义应用应处理或安全忽略不支持的桌面业务消息。

## 版本与验收

本轮 Python 文档核对版本为 0.1.12。安装包、桌面端携带的运行时、源码子模块和机器人固件是不同版本来源；排查时分别记录。

`app check`、命令 ACK、Job 完成和实际设备效果是不同层次的结果。正式使用前仍需验证实际设备与应用组合。
