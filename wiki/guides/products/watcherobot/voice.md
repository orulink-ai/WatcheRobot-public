[English](../../en/products/watcherobot/voice.md) | 简体中文

# 快速创建语音对话

使用 SDK 内置的 voice 模板，在电脑上运行“设备麦克风 → ASR → LLM → TTS → 设备扬声器”。语音业务在 Application 中，不需要改 ESP32 固件，也不是让机器人芯片在本地运行大语言模型。

## 准备条件 {#prepare}

先完成 [SDK 安装和设备配对](../../../../docs/sdk_zh.md)。本页按 SDK **0.1.12** 核对；使用尚未正式发布的 0.1.12 功能时，按安装指南的固定源码方式安装。先执行 `watcherobot app init --help` 和 `watcherobot app configure --help`，确认本机命令可用。

默认模板使用火山 ASR、通义 LLM、火山 TTS。先在 [火山引擎控制台](https://console.volcengine.com/)和[阿里云百炼控制台](https://bailian.console.aliyun.com/)开通自己的服务并取得凭据。服务可能收费，资源、音色和账号权限由供应商决定。音频和文本会发送给配置的服务，不要使用不愿上传的录音进行测试。

## 创建项目 {#create}

在已经激活 SDK 环境的终端执行：

```sh
watcherobot app init my_voice_app --template voice
cd my_voice_app
watcherobot app configure
```

按提示分别填写 ASR、LLM、TTS 凭据；默认 ASR/TTS 各需 `app_id`、`access_token`，LLM 需 `api_key`。输入不回显。向导只保存并本地校验配置，不连接设备或验证云端权限。

## 修改模型与提示词 {#configure}

| 文件 | 内容 |
| --- | --- |
| `config/models/asr.toml` | 语音识别供应商和参数 |
| `config/models/llm.toml` | 大模型、端点和生成参数 |
| `config/models/tts.toml` | 语音合成供应商、资源和音色 |
| `credentials/*.toml` | 本机私有凭据，不上传到 Git 或公开应用 |
| `prompts/system.md`、`config/persona.toml` | 角色提示词和变量 |
| `application/voice.py` | 自己的语音应用组装逻辑 |

更换供应商时，同时修改模型配置和对应凭据。不要把 Key 写进源码、提示词或 `parameters`。配置修改后重启应用生效；SDK 升级不会自动重写已经生成的项目。

只更新某个服务的凭据可执行 `watcherobot app configure --service llm`（也支持 `asr`、`tts`）。环境变量引用必须在 Daemon 启动前可见；新终端设置变量不会自动传给已运行的 Daemon。

## 运行与检查 {#run}

```sh
watcherobot robot status
watcherobot app check .
watcherobot app run
```

保持机器人处于 Desktop Link、设备已连接。对机器人说一句话，等待回答，再追问上一轮内容。默认是“说完 → 回答 → 继续听”的半双工流程，不支持语音抢话打断。不要同时运行另一个占用麦克风、扬声器或 RTC 的应用。

`app check` 通过不代表云模型可调用或声音可听见。验收时分别确认识别文字、模型回答、合成音频与设备实际播放。

## 常见问题 {#help}

- 没有设备：检查 `robot status`、Wi-Fi 和 Desktop Link，先恢复连接。
- 401/403：检查供应商、区域、凭据类型、有效期和资源授权；不要把完整 Key 发到日志或群聊。
- 没声音：先用[原子能力](capabilities.md#audio)区分麦克风采集、TTS 服务和扬声器播放问题。
- 直接执行 `python app.py` 报错：改用 `watcherobot app run`，由 Daemon 注入通道。

完整配置与扩展接口见 [SDK 0.1.12 语音模板参考](https://github.com/orulink-ai/WatcheRobot_python_sdk/blob/cc0a31993b4fe1a607e75ad920ad013e5ddc934a/docs/voice-template.zh-CN.md)。本页做了源码核对，未代你调用付费服务或完成真机语音验收。
