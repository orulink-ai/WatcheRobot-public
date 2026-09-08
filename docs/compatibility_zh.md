<p><a href="compatibility.md">English</a> | <strong>简体中文</strong></p>

# 兼容性

以 [GitHub Releases](https://github.com/orulink-ai/WatcheRobot-public/releases) 中标记为 **Latest** 的 Release、配套清单和发布说明作为当前兼容组合的唯一依据。

## 当前公开组件

| 组件 | 当前公开来源 |
| --- | --- |
| Android | 最新 Release 附带的 APK |
| iOS | 最新 Release 说明中的 TestFlight 入口 |
| 桌面端 | 最新 Release 附带的 Windows x64 安装包或 macOS Apple Silicon DMG |
| Python SDK | 最新 Release 附带的 `watcherobot` wheel |
| ESP32-S3 + Himax | 最新 Release 附带的 `PTL-paired` ZIP |
| SD 卡 | 最新 Release 附带的 `sd-resources` 压缩包 |
| STM32F103 | 最新 Release 附带的 STM32 ZIP |
| 硬件 | `hardware/` 中的当前文件 |

压缩包完整性校验不等同于实机功能验收。每次发布都应单独记录摄像头、视频、音频、方向和持续运行检查。

## 使用顺序

1. 从 Release 页面下载需要的文件。
2. 烧录 STM32F103。
3. 使用 PTL 配套包先烧录 Himax，再烧录 ESP32-S3。
4. 准备 FAT32 SD 卡并写入设备资源。
5. 插卡后上电。
6. 安装同一 Release 中需要的桌面端、移动端或 Python SDK 客户端。
7. 执行首次动作冒烟测试。

组件版本仅记录在 Release 说明和 bundle manifest 中，仓库说明文档不固定版本号。
