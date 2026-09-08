[English](README.md) | 简体中文

# 固件入口

本仓库不公开 ESP32-S3、Himax 和 STM32 固件源码。可烧录固件由 [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest) 提供。

| 目标 | Release 中下载什么 | 使用方式 |
| --- | --- | --- |
| 身体主板 STM32 | STM32 固件包 | ST-LINK V2 连接身体主板，运行仓库烧录脚本 |
| Watcher 头部 | Himax + ESP32-S3 配套包 | USB 连接头部，运行仓库烧录脚本 |
| SD 卡 | SD 资源压缩包 | 用读卡器解压到 SD 卡根目录 |

环境、接线和命令见[烧录指南](../docs/flashing_zh.md)，SD 卡内容见[SD 卡资源说明](../docs/sd-card-assets_zh.md)，开机后按[首次运行检查](../docs/action-test_zh.md)确认设备。

Codex 用户也可以让助手读取 [`skills/watche-release-flash/SKILL_zh.md`](../skills/watche-release-flash/SKILL_zh.md)，再按提示执行烧录。
