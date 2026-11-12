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

## 在 public 仓库中获取 SDK

本仓库已移除原 `firmware/` 目录，通过根目录 **`c-sdk` Git 子模块**引用独立仓库，与 `python-sdk` 相同。旧 Wiki 链接转到当前指引；旧快照仍可从 Git 历史查看。

在本 public 仓库根目录执行以下命令，获取本仓库固定版本的 C SDK：

```sh
git submodule update --init c-sdk
cd c-sdk
```

随后在 `c-sdk` 目录按其上手教程构建。若要同时获取 Python SDK，可在 public 仓库根目录执行 `git submodule update --init --recursive`。子模块固定到具体提交，不会随独立仓库主线自动变化；开发最新版可直接克隆独立仓库。问题反馈与贡献也在独立仓库进行。
