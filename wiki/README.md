# Wiki 维护与自动更新

线上地址：<https://wiki.orulink.ai/>。正文以本仓库为准。
English instructions: [README.en.md](README.en.md).

## 内容来源

- 根 README、`docs/`、硬件/固件/SDK 入口及 `skills/` 是主要内容源，直接编辑原 Markdown。
- `wiki/guides/` 是从原 WatcheRobot-Docs 迁入的客户端、Application、固件维护和排障补充资料；后续在本仓库维护。
- 构建自动生成 `wiki/.generated/`，转换双语链接、引用链接、HTML 图片及 PDF；不提交生成目录。
- 大型硬件生产文件和源码链接继续指向对应 GitHub revision，PDF 和图片随站点分发。
- `wiki/.vitepress/` 只维护主题、导航和构建设置。`/version.json` 记录内容来源 revision。
- Python SDK 子模块的深入说明链接到 SDK 主仓库；正文保留原有公开使用约定。

## 本地验证

所有平台：Node.js 24、Python 3.12+。

```text
npm ci
npm run check
python -m unittest discover -s wiki/tests -p "test_*.py"
npm run docs:dev
```

每次修改正文后重新构建；生成目录不会反向同步到正文。Windows 的四项切换/回滚测试明确跳过，须在 Linux 跑完整测试；Windows 实际安装同样需要符号链接权限。

本地检查指定 revision 的内容（不影响线上发布）：

PowerShell：

```powershell
$env:WIKI_SOURCE_DIR = "/path/to/release-checkout"
$env:WIKI_SOURCE_REF = "watche-system-v0.2.8"
npm run docs:build
Remove-Item Env:WIKI_SOURCE_DIR, Env:WIKI_SOURCE_REF
```

macOS/Linux：

```bash
WIKI_SOURCE_DIR=/path/to/release-checkout WIKI_SOURCE_REF=watche-system-v0.2.8 npm run docs:build
```

## 独立发布

1. 合入 main 后，`wiki.yml` 自动测试、构建该提交的文档。PR 只检查，不发布；手动运行工作流时选择 main。
2. 构建包上传到专用的 [wiki-site 发布通道](https://github.com/orulink-ai/WatcheRobot-public/releases/tag/wiki-site)。它标记为 prerelease 且不设为 Latest，不新建固件版本、不修改固件附件。
3. 每个附件以提交 SHA、工作流 run ID 和 attempt 命名，不覆盖旧包。上传成功后才更新 Release 正文中的 JSON 发布指针，记录仓库、真实提交 SHA、附件名和 SHA256。
4. AWS 每五分钟只检查 `wiki-site`，不再读取固件 Latest。下载、SHA256、归档路径/大小及内嵌版本校验通过后，原子切换 current；HTTPS 健康检查失败则回滚。

`/version.json` 的 `sourceRef` 是实际构建提交，不是 `wiki-site` tag。该 tag 仅作为通道入口，不移动它来表示源码版本。普通固件 Release 的创建或更新不会触发文档部署，已有固件 Release 中的历史 Wiki 附件不再用于在线更新。

发布流程串行执行，并在上传前后核对 main，旧工作流重跑不能覆盖较新提交。上传失败或发布指针不完整时保留线上旧版。通道需允许更新附件和 Release 正文，不支持 immutable releases；不要手动改写 JSON 正文或删除当前指针引用的包。

## AWS 安装与运维

将 `update.py` 安装为 `/opt/watcherobot-wiki/update.py`，把 service/timer 放入 `/etc/systemd/system/`。
首次从固件 Release 模式迁移时，必须先停止 timer 并等待正在运行的 service 结束，再替换服务器的 `update.py`。完成 Linux 测试后手动运行一次 service，核对线上 `version.json`、中英文首页与深层页面，再重新启用 timer。仅合入仓库不会自动更新服务器脚本；未替换时服务器仍执行旧逻辑。
更新器以 `ubuntu` 运行，该用户须拥有 `/www/wwwroot/wiki.orulink.ai/` 的写权限；Nginx 配置、证书仍由管理员管理。
启用 `watcherobot-wiki.timer`，用 `systemctl status watcherobot-wiki.service` 和 `journalctl -u watcherobot-wiki.service` 排查。
网络、限流或下载失败让本次任务退出失败，旧版保持；下个定时周期重试。无需服务器保存 GitHub token。

两平台手动入口参数及退出码一致，通过分发器选择：

```text
npm run wiki:update -- --root /path/to/wiki --health-url https://wiki.orulink.ai/
```

也可分别使用 PowerShell `./wiki/update.ps1` 或 Bash `bash wiki/update.sh`，参数同上；`--help` 查看说明。
Linux systemd 直接调用 Python 核心，无需安装 Node.js 构建环境。

版本存放 `releases/<release-id>-<digest>/`；current 为相对符号链接。更新器使用进程文件锁防止并发，旧目录保留供回滚。
回滚：先停止 timer，将 current 原子切回先前目录并验证 HTTPS。优先在 main 提交修复或 revert，让工作流发布新的包，再启用 timer；紧急情况下可将 `wiki-site` 的 JSON 指针恢复为已验证的旧包信息。不要修改固件 Latest；如果指针仍是故障包，重新启用 timer 会再次部署它。
不要手动修改正在服务的 release 目录。
