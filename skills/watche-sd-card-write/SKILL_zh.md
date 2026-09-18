---
name: watche-sd-card-write-zh
description: 通过读卡器和仓库跨平台脚本下载、校验并写入 WatcheRobot 官方 SD 卡资源。
---

[English](SKILL.md) | 简体中文

# WatcheRobot SD 卡写入

先读 [SD 卡资源说明](../../docs/sd-card-assets_zh.md)。只使用读卡器和仓库脚本；使用烧录指南中的专用非 base Conda 环境，不向其他 Python 环境安装内容，也不修改其他环境。

写入前确认准确的 SD 卡盘符或挂载点。Windows 使用 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd --drive "E:\"`；macOS/Linux 使用 `bash tools/flash.sh sd --drive "/Volumes/WATCHE"`。示例路径必须换成实际 SD 卡，不能猜测盘符或选择系统盘。

不写 `--package` 时，脚本自动下载最新官方包。使用 Release 中的资源包时，在命令末尾添加 `--package <watche-sd-resources-*.tar.gz>`。不要手工解压发布归档；脚本会校验并转换成设备使用的 `watche/` 布局。

脚本必须确认 FAT32、介质可写、压缩包清单、逐文件哈希和最终目录，并通过 staging 切换官方资源，同时保留用户作品。不得格式化卡、删除无关文件；只有用户明确要求丢弃未完成的设备事务时才可使用 `--force`。

只有退出码为零且最后出现 `Installed ... successfully` 才能报告成功。随后提醒用户安全弹出 SD 卡，并在机器人断电状态下插回头部。
