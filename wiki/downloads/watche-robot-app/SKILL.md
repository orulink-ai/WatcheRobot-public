---
name: watche-robot-app
description: 根据用户需求，使用 WatcheRobot Python SDK 创建、修改、运行和排查机器人应用。包含按需查询的能力说明、完整应用模板和环境检查工具，无需用户提供内部仓库或飞书文档。
---

# WatcheRobot 机器人应用

目标：用户只描述想做什么，你仅凭本 skill 的流程、模板和能力说明，交付符合需求的 Python 应用。先完成可运行的最小功能，再按需要扩展；不要要求用户先读 SDK 文档。

应用在电脑上运行，通过 SDK Daemon 控制机器人。保持 `ApplicationContext.from_environment()` 入口，以 `watcherobot app run` 启动；不要直接运行 app.py 或另建设备连接。

## 按这四步执行

1. **选模板。** 将需求整理成“触发 → 行为 → 结果”。启动执行一次选 `photo`；持续监听触摸选 `input`；点击网页按钮选 `button`。其他行为复用对应触发模板，只替换业务函数。已有项目保留原结构，不重新初始化。用户只问功能时仅介绍能力。
2. **准备。** 读 [开发步骤 D01—D03](references/开发步骤.md)，用本 skill 的 `watche-app` 平台入口执行一次 `prepare`，默认安装正式 PyPI 最新兼容稳定版；已有项目跳过 prepare。检查实际生成的 `sdk-capabilities.json`。缺少所需方法时按 [资源与兼容](references/资源与兼容.md) 处理，不猜版本、不静默换功能。
3. **加能力。** 在下表定位章节，读 [共同前提 C00](references/机器人能力.md)，再直接打开下表链接，只读该能力文件。保留模板入口、参数注释和清理代码；把实际使用的方法加入项目 `sdk-requirements.json`。遇到资源 ID 先读资源与兼容，不填猜测值。
4. **检查并交付。** 执行同一入口的 `check`，失败则修正，不绕过。按开发步骤 D06 在已有授权内托管运行。交付项目路径、完整启动命令和使用方法，分清本地检查、设备结果与实际效果。无设备则交付代码并标明待实机验证。

不需要同时读完全部手册。默认只读开发步骤和本次用到的能力；RTC、语音、发布属于条件流程。

需要核对源码、补充教程或排查未覆盖的问题时，读 [SDK 源码与飞书资料](references/源码与资料.md)，按任务只打开对应资源。默认安装 pip 包；查阅源码不会自动切换安装来源。

## 能力目录

| 用户想做什么 | 读取章节 | 调用选择 |
|---|---|---|
| 转头、连续调角度、播放动作 | [C01 运动](references/capabilities/C01-运动.md) | `move_to` / `set_target` / `play_action` |
| 常亮、呼吸灯、闪灯 | [C02 灯光](references/capabilities/C02-灯光.md) | `set_color` / `play_effect` |
| 播放已有动画、官方表情 | [C03 动画和官方表情](references/capabilities/C03-动画和官方表情.md) | `animation.play` / `expressions.play_official` |
| 编程画眼睛、装饰、嘴型跟随 | [C04 程序化表情](references/capabilities/C04-程序化表情.md) | 参数 / 像素 / 矢量；嘴型须查安装包 |
| 播放设备音效、文件、PCM | [C05 声音输出](references/capabilities/C05-声音输出.md) | `play` / `play_file` / `play_pcm` / `open_stream` |
| 录音、实时收音、原始音频包 | [C06 麦克风](references/capabilities/C06-麦克风.md) | `record_pcm` / `open_pcm` / `open` |
| 拍照、带动画和快门反馈拍照 | [C07 拍照](references/capabilities/C07-拍照.md) | `capture` / `capture_with_feedback` |
| 选择视觉模型、识别物体、返回画面 | [C08 设备视觉](references/capabilities/C08-设备视觉.md) | 模型查询 / 切换 / 推理 / 推理预览 |
| 跟随人脸、查看跟随画面 | [C09 人脸跟踪](references/capabilities/C09-人脸跟踪.md) | `start` / `open_preview`；会控制运动 |
| 触摸、屏幕点击、滚轮触发 | [C10 实体输入](references/capabilities/C10-实体输入.md) | 按 `event.source` 分流 |
| 录像、录音视频、下载、转换文件 | [C11 录制](references/capabilities/C11-录制.md) | 设备 / 主机 / 屏幕预览 / 下载转换 |
| 实时音视频会话 | [C12 RTC](references/capabilities/C12-RTC.md) | 音频 / 视频 / 音视频；需媒体客户端 |
| 播放组合行为、作品、作品片段 | [C13 行为和作品](references/capabilities/C13-行为和作品.md) | `behavior.play` / `works.play` / `play_expression` |
| 对接 Watcher Desktop 按钮和消息 | [C14 Desktop 与应用界面](references/capabilities/C14-Desktop-与应用界面.md) | `app.desktop`；自建网页使用应用自己的接口 |
| 查询设备能力、资源、任务结果 | [C15 状态与任务](references/capabilities/C15-状态与任务.md) | `supports` / 资源快照 / `Job` |
| 语音聊天、接入 ASR/LLM/TTS | [C16 语音应用](references/capabilities/C16-语音应用.md) | 自选云服务用 `voice`；设备已有语音配置用 `speech` |

## 写代码时保留这五条

- `app.robot`/`app.rtc` 的阻塞操作通过模板的 `finish_thread` 执行；`app.desktop.send/receive` 使用 await。持续任务响应 `shutdown_requested`；清理是尽力而为，托管退出预算见 D06。
- 同一相机、云台、音频或屏幕资源由本应用串行调度。保留 `device_session`/with/finally；超时不等于已停止，明确拒绝启动时不能停止其他所有者。
- 普通拍照和带反馈拍照是不同接口；用户指定反馈时不得偷偷降级。按实际安装包检查所需接口，缺失时查资源与兼容。
- Application 的业务必须走 Daemon 注入的受管通道；不改 Daemon 按消息类型分流。自建网页只调用本应用接口。
- 沿用已有授权；不擅自接管别人的应用、刷固件、删除资源或发布。安装需网络，真机效果需设备，云服务需用户自己的凭据；不把密钥写入代码和日志。

## 维护范围

SDK 更新时，只同步影响应用能做什么、调用怎么写、环境怎么准备及资源怎么释放的变化。直接修正对应流程、模板和能力说明，不追加 SDK 更新日志、内部实现历史或官方演示操作教程。
