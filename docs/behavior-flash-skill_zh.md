<p><a href="behavior-flash-skill.md">English</a> | <strong>简体中文</strong></p>

# 行为资源现场检查清单

准备或更换 WatcheRobot SD 卡时使用本清单。

## 输入

- WatcheRobot 仓库
- 通过读卡器连接到电脑的 SD 卡
- 最新 Release 中的 `sd-resources` `.tar.gz` 压缩包

## 操作步骤

1. 从 Watcher 头部取出 SD 卡，用读卡器连接电脑并确认挂载路径，例如 Windows 的 `E:\`、macOS 的 `/Volumes/WATCHER_SD` 或 Linux 的 `/media/$USER/WATCHER_SD`。不能通过 Watcher 的 USB 或串口写卡。
2. 将下载的压缩包解压到临时目录。
3. 确认解压结果包含：

```text
assets/
official_catalog.json
resource_manifest.json
```

4. 将 SD 卡格式化为 FAT32，把解压内容复制到卡根目录，不要增加额外的外层目录。
5. 安全弹出 SD 卡。
6. 从读卡器取出已安全弹出的 SD 卡，在机器人断电时插回 Watcher 头部。
7. 执行[首次动作冒烟测试](action-test_zh.md)。

## 通过标准

- SD 卡根目录包含 `assets/`、`official_catalog.json` 和 `resource_manifest.json`。
- `assets/anim/` 中包含 `.animpack` 文件。
- 启动时没有动画资源缺失错误。
- 首次动作测试至少可以触发一个行为。

如果资源未识别，请先检查 FAT32 格式和目录层级。
