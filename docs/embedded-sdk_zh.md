[English](embedded-sdk.md) | 简体中文

# 嵌入式 C SDK（开发预览）

C SDK 已独立维护在公开仓库 **[WatcheRobot_c_sdk](https://github.com/orulink-ai/WatcheRobot_c_sdk)**。源码、示例、依赖和使用说明以该仓库为准；无需访问私有仓库。

## 选择开发路线

| 你想做什么 | 从这里开始 |
| --- | --- |
| 在电脑上编写应用，控制现有机器人固件 | [Python SDK](sdk_zh.md) |
| 在 ESP32-S3 上编写 C 应用，使用屏幕、声音、相机、灯光或运动 | [C SDK 中文首页](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/README_zh.md) |

两条路线独立，使用 C SDK 不要求先安装 Python SDK 或 Runtime/Daemon。

## 从独立仓库开始

1. 按[第一个 C 应用教程](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/docs/getting-started_zh.md)安装指定 ESP-IDF、克隆完整独立仓库，运行不启动身体运动的 `hello_display` 示例。构建和烧录命令均在 **`WatcheRobot_c_sdk` 仓库根目录**执行，不在本 public 仓库执行。
2. 按[功能指南](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/docs/features.md)查找接口，从[示例目录](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/examples/README.md)选择工程。
3. 需要整机体检时，再看[硬件体检示例](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/examples/hardware_check/README_zh.md)。

## 烧录与运动前必读

- 首次完整烧录会替换原应用和分区表，可能覆盖设备配置。先确认[硬件、配套固件与备份要求](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/docs/hardware-compatibility.md)，准备可用的恢复镜像；配套版本未确认前不要烧录。
- 恢复官方功能按[完整恢复流程](flashing_zh.md#full)操作，不能只写回旧应用。
- 舵机不能读取实际转到的角度；命令坐标和 ACK/DONE 不证明已到位或停止。运动前清空完整行程，遵守[身体控制 API 的安全说明](https://github.com/orulink-ai/WatcheRobot_c_sdk/blob/main/docs/api/body.md)，并观察实际动作。
- C SDK 仍是**开发预览**，独立建仓不代表稳定发布、配套固件完整验收或实机安全认证。

## 旧快照说明

本仓库的 `firmware/sdk/` 保留为历史开发预览快照，不再作为当前开发入口；旧页面链接继续可访问。不要混用该快照的路径、依赖和命令与独立仓库的源码。当前问题反馈和贡献请到 [C SDK 独立仓库](https://github.com/orulink-ai/WatcheRobot_c_sdk)。
