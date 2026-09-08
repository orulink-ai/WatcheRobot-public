[English](sd-card-assets.md) | 简体中文

# SD 卡资源

从 [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest) 下载 SD 资源压缩包。用读卡器将 FAT32 格式的 SD 卡连接电脑，把压缩包内容解压到卡的根目录，再将卡插回 Watcher 头部。

当前 Release 的压缩包解压后结构如下：

```text
SD 卡根目录/
├─ assets/
│  ├─ actions/          动作描述文件
│  ├─ anim/             表情动画资源
│  └─ sfx/              音效资源
├─ fixed_states.json
├─ official_catalog.json
└─ resource_manifest.json
```

这些目录和文件应直接位于 SD 卡根目录，不要在外面再套一层压缩包文件夹。
