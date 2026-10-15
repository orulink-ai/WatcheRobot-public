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

指定正式 Release 内容（工具配置保持当前 checkout）：

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

## 正式发布

1. 合入文档变更至 main；通过正常产品发布流程发布非草稿、非预发布的 GitHub Release。
2. `wiki.yml` 测试后构建该 tag 的正文，向同一个 Release 上传 `watcherobot-wiki.tar.gz` 与同名 `.sha256` 文件。
3. AWS 每五分钟检查 GitHub 的 **Latest stable Release**。产物暂未生成时保留当前站点，下轮重试。
4. 下载、SHA256、归档路径/大小、版本身份验证通过后，原子切换 current；HTTPS 健康检查失败会回滚。

main 的普通 push 和 PR 只验证，不更新线上。未设为 Latest 的稳定 Release 也不会覆盖线上。
仅选择现有正式 Release 运行 `workflow_dispatch` 可补齐历史版本的 Wiki 产物，不创建或改写产品 tag。
旧 tag 没有 `wiki/guides/` 时不显示补充指南，以该 tag 已发布的正文为准。
Release 附件被覆写后，下一轮会按新 checksum 更新；无需变更版本号，但建议正式变更使用新 Release。
仓库若启用 immutable releases，应在锁定前准备完整资产，不能依赖发布后附加附件。

## AWS 安装与运维

将 `update.py` 安装为 `/opt/watcherobot-wiki/update.py`，把 service/timer 放入 `/etc/systemd/system/`。
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
回滚：先停止 timer，将 current 原子切回先前目录，验证 HTTPS；重新启用前需把 GitHub Latest 指向期望版本，否则下一轮会再次跟随 Latest。
不要手动修改正在服务的 release 目录。
