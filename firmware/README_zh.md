<div align="center">

<p><a href="README.md">English</a> | <strong>简体中文</strong></p>

</div>

# 固件

本仓库不提供固件源码。请从 [GitHub Releases](https://github.com/orulink-ai/WatcheRobot-public/releases) 中标记为 **Latest** 的 Release 下载 STM32 ZIP、ESP32-S3 + Himax `PTL-paired` ZIP、SD 卡资源压缩包。

组装完成后按以下顺序处理：

1. 断电检查接线和电源极性。
2. 通过 ST-LINK/SWD 烧录 STM32F103。
3. 按配套 ZIP 内的 Skill 先烧录 Himax，再烧录 ESP32-S3。
4. 准备并插入 FAT32 SD 卡。
5. 上电并执行首次启动检查。

烧录入口见[固件烧录指引](../docs/flashing.md)，附件选择见[下载说明](../docs/downloads.md)。
