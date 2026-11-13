[English](README.md) | 简体中文

# WatcheRobot

![WatcheRobot 桌面机器人](docs/images/watcher-robot-render.png)

## 产品介绍

WatcheRobot 是一款可编程的桌面机器人，面向桌面陪伴、互动展示和机器人应用开发。它通过表情、动作、灯光和声音表达状态，也能读取触摸、相机和麦克风输入，让你把自己的创意做成可以看见、听见和互动的应用。

- **表达**：屏幕动画与代码表情、头部水平转动与俯仰、灯色与灯效、音效与音频播放。
- **感知**：拍照、麦克风采集、背部触摸、屏幕点击和滚轮输入。
- **互动**：人脸跟随、录音录像、实时音视频；语音对话通过应用接入 AI 服务。
- **创造**：用 Python SDK 编写电脑端应用，用 C SDK 开发预览探索设备端逻辑，或按公开资料自行制作整机。

[查看全部功能与 API](wiki/guides/products/watcherobot/capabilities.md)

## 快速开始

已有整机，从连接和使用开始：

1. **安装客户端**：从[设备使用指南](wiki/guides/products/watcherobot/device-guide.md#install-app)获取电脑或手机安装包。
2. **配置网络**：用手机 App 扫描设备二维码，完成 **2.4 GHz Wi-Fi** 配网（**不支持 5 GHz**），见[配网说明](wiki/guides/products/watcherobot/device-guide.md#wifi)。
3. **连接机器人**：电脑选择 Desktop Link，手机选择 Phone Control，见[连接说明](wiki/guides/guide/getting-started.md)。
4. **开始体验**：按[首次运行检查](docs/action-test_zh.md)检查表情、灯效和安全范围内的动作。

## 开发与制作

**推荐：**[用 AI Agent 创建应用](wiki/guides/guide/create-with-agent.md)。复制一句提示词，把你的想法交给 Agent；完整 Skill ZIP 可直接下载。

| 你想做什么 | 从这里开始 |
| --- | --- |
| 用 Python 创建第一个机器人应用 | 推荐 [AI Agent + Skill 快速开始](wiki/guides/guide/create-with-agent.md)；[手动开发](docs/sdk_zh.md) |
| 搭建语音对话应用 | [快速创建语音对话](wiki/guides/products/watcherobot/voice.md) |
| 编写设备端程序 | [C SDK 独立仓库（开发预览）](https://github.com/orulink-ai/WatcheRobot_c_sdk)；[C SDK 使用指引](docs/embedded-sdk_zh.md) |
| 自己制作一台机器人 | [复现指引](wiki/guides/guide/reproduction.md) |
| 下载硬件、BOM、固件和资源 | [资源与下载](hardware/README_zh.md) |

## 获取机器人

[京东预购 WatcheRobot](https://item.jd.com/100326823811.html)（中国大陆）。预购规则和预计发货时间以商品页为准；海外购买渠道即将上线。

## 交流与支持

[微信与 Discord 社区、问题反馈和贡献入口](wiki/guides/products/watcherobot/community.md) · [故障排查](wiki/guides/troubleshooting/index.md)

## 开源与许可

公开内容包括硬件生产资料、机械模型、Python SDK、烧录工具及文档；[C SDK](https://github.com/orulink-ai/WatcheRobot_c_sdk) 为开发预览，源码、示例与使用说明见 SDK 文档。官方 ESP32-S3 应用、STM32、Himax、客户端和服务端实现不在本仓库公开。

本仓库使用 [GPL-3.0](LICENSE)，子项目和第三方组件以各自许可证为准。
