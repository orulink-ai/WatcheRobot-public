<p><a href="CONTRIBUTING.md">English</a> | <strong>简体中文</strong></p>

# 贡献指南

感谢你一起完善 WatcheRobot。这个仓库用于公开硬件资料、机械模型、Python SDK、使用文档、烧录工具和 Release 资产说明。

## Fork 和分支

1. Fork `orulink-ai/WatcheRobot-public`。
2. Clone 你自己的 fork。
3. 创建一个聚焦的工作分支。

推荐分支命名：

| 工作类型 | 分支格式 |
| --- | --- |
| 文档 | `docs/<topic>` |
| SDK | `sdk/<topic>` |
| 硬件资料 | `hardware/<topic>` |
| Release 文档或清单 | `release/<topic>` |
| 工具 | `tools/<topic>` |

一个 PR 尽量只解决一个明确问题。

## PR 标题

标题使用简短前缀：

- `docs: ...`
- `sdk: ...`
- `hardware: ...`
- `release: ...`
- `tools: ...`

示例：

- `docs: add SD-card behavior asset checklist`
- `hardware: document spare parts for wireless charging base`
- `sdk: add camera capture example`

## 不要提交的内容

不要提交：

- ESP32-S3 或 STM32F103 固件源码
- Release 产物：`.bin`、`.zip`、`.exe`、`.msi`、`.dmg`、`.apk`、`.aab`
- 本地构建输出或生成的固件镜像
- Wi-Fi 凭据、API key、token、私钥或 `.env` 文件
- 本地机器路径、私人串口记录、只属于现场台架的 COM 口日志
- 闭源 App、Server、Desktop 包的源码

二进制发布物应该上传到 GitHub Releases，不进入 Git 历史。

## 文档更新要求

如果你的改动影响安装、刷写、行为资源、硬件资料、SDK 或 Release 资产，请同步更新对应文档：

- 快速开始：`README_zh.md`
- 固件和烧录：`firmware/README_zh.md`、`docs/flashing_zh.md`
- SD 卡和首次运行：`docs/sd-card-assets_zh.md`、`docs/action-test_zh.md`
- SDK：`docs/sdk_zh.md`
- 版本与配套关系：`docs/versions_zh.md`、`docs/compatibility_zh.md`
- 硬件与 BOM：`hardware/README_zh.md`、`hardware/pcb/spares_zh.md`

同时更新对应英文文档。

## 提交 PR 前的验证

按改动范围运行检查：

- 仅文档改动：检查中英文入口和本地链接。
- SDK 改动：在 `python-sdk/` 下运行 `python -m pytest`。
- 烧录工具改动：运行 `python -m pytest tools/tests -q`，并按改动范围验证目标设备。
- 使用流程改动：复核 `docs/action-test_zh.md`。
- 硬件资料改动：确认 BOM、CPL、Gerber、原理图、Layout 和可编辑源文件名称对应同一块板。

如果你的环境缺少某个工具，请在 PR 的 Test Plan 中明确说明。
