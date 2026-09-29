[English](sd-card-assets.md) | 简体中文

# SD 卡资源

## 1. 准备 SD 卡

通过读卡器连接 SD 卡，使用 **FAT32，分配单元大小 512 字节**。已符合要求的卡无需重新格式化。

Windows 可在「此电脑」中右键 SD 卡 →「格式化」，选择「FAT32」和「512 字节」，点击「开始」。**格式化会删除卡内全部文件，包括原有资源和用户作品，请先备份。**

也可以在管理员 PowerShell 中激活专用 Conda 环境，从仓库根目录运行格式化脚本（将 `E:\` 换成实际 SD 卡）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd-format --drive "E:\"
```

核对显示的盘符和容量，输入 `E:` 确认，直接回车取消。脚本仅支持 Windows 可移动盘，完成后检查 FAT32 和 512 字节分配单元；若系统不支持该容量与格式组合，会报错停止。

## 2. 写入资源

SD 资源包是发布归档，由脚本转换为设备目录。在已激活的专用 Conda 环境中从仓库根目录运行：

```powershell
# Windows：自动下载最新官方资源
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash.ps1 sd --drive "E:\"
```

```sh
# macOS/Linux：自动下载最新官方资源
bash tools/flash.sh sd --drive "/Volumes/WATCHE"
```

若已下载 Release 中的 `watche-sd-resources-*.tar.gz`，在相应命令末尾添加 `--package "压缩包路径"`。脚本会校验压缩包，保留已有作品，并安装为设备需要的结构：

```text
SD 卡根目录/
└─ watche/
   ├─ assets/
   │  ├─ actions/                 动作资源对象
   │  ├─ anim/                    表情动画对象
   │  └─ sfx/                     音效对象
   ├─ official/current/
   │  ├─ official_catalog.json    官方资源目录
   │  ├─ fixed_states.json        固定状态映射
   │  └─ resource_manifest.json   资源完整性清单
   ├─ works/
   │  └─ works_catalog.json       用户作品目录
   ├─ system/
   │  ├─ layout.json              SD 布局标识
   │  └─ accepted_official.json   已安装官方资源记录
   └─ staging/                    安装事务临时目录
```

日志最后出现 `Installed ... successfully` 表示电脑端写卡和校验完成。安全弹出卡，在机器人断电状态下插回头部；上电进入正常界面并成功播放一个表情后，设备端读取验收完成。
