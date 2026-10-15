# public 文档站与 Release 更新

- 创建：2026-10-15，Asia/Shanghai；作者：KID；执行：Codex。
- 用户需求：将 wiki.orulink.ai 融入 WatcheRobot-public，以该仓库更全面的文档为准，跟随正式 Release 自动更新。
- 状态：进行中；独立分支 codex/public-wiki-release，避免混入原本地 checkout 的 SDK 工作。
- 规则：目标仓库没有 AGENTS.md 和开发档案规范/模板；已读取 Workspace 对应文件，按其要求记录；未找到本任务已有档案。

## 目标、边界及验收

直接以根 README、docs、硬件与固件入口为站点内容源，生成双语导航、搜索、图片/PDF，保留原 Markdown 的 GitHub 阅读方式。仅构建阶段生成网站目录，不维护两份正文。迁入现有站点主题。

Release published 自动构建可下载的 wiki 产物；服务器仅向外拉取公开产物，校验与原子切换，失败保留旧版，不向 Actions 提供个人 SSH 私钥或开放额外 SSH 来源。服务器定时器与部署脚本具备 Windows/macOS/Linux 对称运行入口。

验收包括内容链接转换与资源、双语构建、正式版本过滤、校验失败/恶意归档拒绝、原子切换、重复执行幂等、失败保留旧版，及生产 HTTPS 深层页面。

## 已知事实与待办

- public 最新正式版本为 watche-system-v0.2.8；现有 Release 未包含 wiki 产物。
- 原 Wiki 的静态站点在 AWS 上已可访问；此次将替换内容来源并补自动发布链路。
- 需先完成测试与本地可审查改动，再发布分支/PR；不自行制造产品 Release。
- 历史站点部署档案位于独立 WatcheRobot-Docs 仓库，本次部分替代其内容源和手动发布策略，保留其服务器/Nginx/证书调查事实。

## TDD 与证据

- Red：内容转换与更新器测试最初因目标模块尚未实现失败。首次实际构建又发现带行内代码标签的 Markdown 链接未转换，死链门禁拒绝发布；补覆盖用例并修复解析，未关闭死链检查。
- Green：Windows 上 6 项 Node 测试通过，48 页生产构建通过。Python 在 Windows 3 项通过、4 项因符号链接权限跳过；在真实 Linux 服务器完整 7 项通过，覆盖幂等、摘要不匹配、路径穿越/链接归档拒绝、版本身份、首次与已有版本的健康回滚。
- 双平台更新入口经 npm 分发器实测 --help 返回 0、错误参数返回 2；Linux 入口还将随服务安装验证。
- 从旧站迁入的双语快速开始仍含旧组织下载链接，已统一改为 public 最新 Release；正文修改以人工对照为验收，不伪称代码 TDD。
- 本机 OpenSSH 在密钥交换时持续超时，改用已安装 Paramiko，保持 known_hosts 校验与同一本机密钥后登录成功；没有弱化服务器认证或扩大防火墙。

## 设计与边界

- PR/main 仅构建验证；正式 Release 才生成可安装产物。用 gh 验证 Release 非草稿/非预发布，按明确 tag checkout 正文，GitHub Actions 上传两个 Wiki 附件。
- AWS 每五分钟只读 Latest stable API；无需 PAT，无需新增 Actions SSH 白名单。缺附件时保持当前页面，失败不切换。
- 归档拒绝链接、绝对路径、上级路径和超额解压；SHA256 与版本 manifest 通过后原子切换，HTTPS 检查失败恢复前一链接；旧目录保留。
- 不制造新产品 Release，不修改现有固件附件，不混入原 checkout 的硬件 SDK 未提交工作。
- GitHub 官方发布事件与公开 Release API 说明已核对：<https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#release>、<https://docs.github.com/en/rest/releases/releases#get-the-latest-release>。

## 部署与交付记录

2026-10-15｜KID / Codex：实现已提交为 33e9b521c46ba45555528d31cd51784ef20a4a85，通过 gh 发布独立分支，创建草稿 PR：https://github.com/orulink-ai/WatcheRobot-public/pull/7 。整合站点已部署至 https://wiki.orulink.ai ，浏览器确认首页与完整导航正常。

- 生产通过 HTTPS 验证首页、双语烧录、硬件、固件 SDK、旧入门与 SDK 路由，以及图片、PDF、version.json；不存在路径返回 404。
- 线上 version.json 指向上述实际构建提交。产物 SHA256 为 abcac98b3e0e4ebd324f02e1227d88cfe3e2e3a7370df2fd2254c30882c19e41；通过更新器安装与健康检查，旧版本保留。
- AWS 已安装受限 ubuntu 用户运行的 systemd service/timer，每五分钟检查公开正式 Release；配置验证及 Nginx 检查通过。实际执行确认现有 Release 缺少 Wiki 附件时保留当前站点。
- GitHub Wiki 构建运行 38683467639 成功，PR 发布任务按设计跳过；原仓库检查运行 38683467599 全部通过，包括双平台烧录工具与 Python 3.10/3.11/3.12。
- Linux 七项更新器测试及 Shell 入口验证通过。此记录为部署证据补充，无运行时代码变化，不另增镜像实现测试。

交付状态：当前整合站点可用，服务器自动检查已启用，PR 待审查合入。默认分支的 Release 发布工作流尚未启用；尚未执行真实 Release Wiki 附件上传和自动下载的完整生产验证。没有新建产品版本，也没有更改现有固件附件。合入后可在正式 Release 发布时验证，或按维护说明补齐指定历史正式版本。
