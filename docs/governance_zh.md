<p><a href="governance.md">English</a> | <strong>简体中文</strong></p>

# 仓库规则

- Python SDK 使用 `python-sdk/` 子模块。
- 硬件资料放在 `hardware/`。
- C SDK 使用 `c-sdk/` 子模块，固定引用公开的 [WatcheRobot_c_sdk](https://github.com/orulink-ai/WatcheRobot_c_sdk)，不在本仓库重复维护源码；许可与第三方署名以独立仓库为准。
- 官方 ESP32 应用、STM32F103、Himax、App、服务端和桌面端实现不放入本仓库。
- 固件、客户端安装包和 SD 卡资源只上传到 GitHub Releases。
- 私有路径、账号、凭据和内部日志不得写入公开文档。
