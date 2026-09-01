# WatcheRobot 固件烧录 Skill

这个 README 是给 AI 助手读取的烧录流程说明。用户不需要手动理解每个脚本参数；当用户希望烧录 WatcheRobot 固件时，AI 应先阅读本文，再根据当前机器、串口和 Release 资源执行操作。

## 用户可以这样说

```text
请阅读 tools/flashing/README_zh.md，帮我烧录 WatcheRobot。
使用最新 Release 中的 ESP32-S3 固件、STM32F103 固件和 SD 卡资源，识别当前串口，烧录后帮我查看启动日志。
```

## AI 需要确认的输入

- 当前仓库路径。
- 目标硬件：ESP32-S3、STM32F103、SD 卡资源，或完整套装。
- Release 版本。未指定时优先使用最新 WatcheRobot Release。
- 本机串口。未指定时先列出串口并结合设备插拔变化判断。
- 是否需要烧录后监视日志。
- SD 卡盘符或挂载路径。

## 推荐流程

1. 确认 Git 工作区，避免把 Release ZIP、日志、临时文件提交到仓库。
2. 从 GitHub Release 获取同一版本套装中的资源，不混用不同版本。
3. 校验 `SHA256SUMS.txt` 或 Release manifest。
4. 识别 ESP32-S3 串口；必要时让用户重新插拔设备辅助判断。
5. 使用仓库内烧录工具烧录 ESP32-S3 Release ZIP。
6. 如果用户要求 STM32F103，使用同一 Release 中的 STM32 发布包，并按用户当前调试器工具处理。
7. 如果用户要求 SD 卡资源，将同版本 SD 卡资源写入目标 SD 卡。
8. 烧录后抓取启动日志，给出成功、失败或需要人工检查的结论。

## 可用工具入口

- `tools/flash-release.cmd`：Windows 下 ESP32-S3 Release ZIP 烧录快捷入口。
- `python -m tools.win_flasher`：ESP32-S3 Release ZIP 烧录、串口枚举和监视日志。
- `docs/flashing.md`：串口驱动、端口识别和平台差异说明。
- `docs/sd-card-assets.md`：SD 卡资源目录结构和写入说明。

## 输出要求

AI 完成后应返回：

- 使用的 Release 版本和资源文件名。
- 使用的串口或设备别名。
- 烧录命令是否成功。
- 启动日志中的关键结果。
- 是否还需要用户检查硬件、电源、按键或 SD 卡。
