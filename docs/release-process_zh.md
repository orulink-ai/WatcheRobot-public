<p><a href="release-process.md">English</a> | <strong>简体中文</strong></p>

# 发布规则

发布文件通过 GitHub Releases 分发，不提交到 Git 历史。

完整 Release 应包含：

- ESP32-S3 + Himax 配套 ZIP
- STM32 ZIP
- SD 卡资源压缩包
- Python SDK wheel
- Windows、macOS 和 Android 安装包
- iOS TestFlight 入口
- bundle manifest、校验文件和发布说明

配套 ZIP 已包含两个芯片的固件、工具和 Skill 时，不再单独上传重复附件。
