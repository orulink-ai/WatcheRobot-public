<p><a href="sd-card-assets.md">English</a> | <strong>简体中文</strong></p>

# SD 卡资源

从 Latest Release 下载 `sd-resources` `.tar.gz`。

1. 从 Watcher 头部取出 SD 卡，用读卡器连接电脑。不能通过 Watcher 的 USB 或串口写入这张卡。
2. 下载 Release 中的 SD 卡资源压缩包。
3. 备份卡内需要保留的文件，再将 SD 卡格式化为 FAT32。
4. 把压缩包内容直接解压到卡根目录，不要多套一层文件夹。
5. 确认根目录包含：

```text
assets/
official_catalog.json
resource_manifest.json
```

6. 安全弹出读卡器中的 SD 卡，在机器人断电时插回 Watcher 头部。
7. 上电后执行[首次启动检查](action-test_zh.md)。

如果资源未识别，先检查FAT32 格式和目录层级。
