English | [简体中文](../../../products/watcherobot/voice.md)

# Create a voice conversation

Use the SDK's built-in voice template on your computer: robot microphone → ASR → LLM → TTS → robot speaker. Voice logic belongs to the Application; no ESP32 firmware changes are required, and the robot chip does not run the language model locally.

## Prerequisites {#prepare}

Complete [SDK installation and pairing](../../../../../docs/sdk.md). This guide targets SDK **0.1.12**. Use the pinned source installation in that guide when using unreleased 0.1.12 features. Check `watcherobot app init --help` and `watcherobot app configure --help` before continuing.

The default template uses Volcengine ASR, Qwen LLM, and Volcengine TTS. Enable your services and obtain credentials in the [Volcengine console](https://console.volcengine.com/) and [Alibaba Cloud Bailian console](https://bailian.console.aliyun.com/). Providers determine pricing, resource access, and voice availability. Audio and text are sent to the configured services; do not test with recordings you do not want to upload.

## Create a project {#create}

In a terminal with the SDK environment activated:

```sh
watcherobot app init my_voice_app --template voice
cd my_voice_app
watcherobot app configure
```

Enter ASR, LLM, and TTS credentials. Default ASR/TTS each require `app_id` and `access_token`; LLM requires `api_key`. Input is hidden. The wizard saves and validates configuration locally; it does not connect a robot or verify cloud permissions.

## Configure models and prompts {#configure}

| File | Purpose |
| --- | --- |
| `config/models/asr.toml` | Recognition provider and parameters |
| `config/models/llm.toml` | Language model, endpoint, and generation parameters |
| `config/models/tts.toml` | Speech provider, resource, and voice |
| `credentials/*.toml` | Private local credentials; never upload to Git or public apps |
| `prompts/system.md`, `config/persona.toml` | Persona prompt and variables |
| `application/voice.py` | Your application composition logic |

When changing providers, update both model configuration and credentials. Never put keys in source code, prompts, or `parameters`. Restart the application after changes. SDK upgrades do not rewrite generated projects.

To update one service, run `watcherobot app configure --service llm` (also `asr` or `tts`). Environment-variable references must be available before the Daemon starts; variables set in a new terminal do not reach an existing Daemon automatically.

## Run and verify {#run}

```sh
watcherobot robot status
watcherobot app check .
watcherobot app run
```

Keep the connected robot in Desktop Link. Say a sentence, wait for a reply, then ask a follow-up. The default half-duplex flow is speak → answer → listen again, without voice barge-in. Do not run a competing application using the microphone, speaker, or RTC.

Passing `app check` does not prove cloud access or audible playback. Verify recognized text, model replies, synthesized audio, and physical speaker output separately.

## Troubleshooting {#help}

- No device: check `robot status`, Wi-Fi, and Desktop Link; restore the connection first.
- 401/403: check provider, region, credential type, expiry, and resource access. Never post full keys in logs or chat.
- No audio: use the [audio primitives](capabilities.md#audio) to distinguish capture, TTS, and playback failures.
- `python app.py` fails: use `watcherobot app run` so the Daemon injects channels.

See the [SDK 0.1.12 voice template reference (Chinese)](https://github.com/orulink-ai/WatcheRobot_python_sdk/blob/cc0a31993b4fe1a607e75ad920ad013e5ddc934a/docs/voice-template.zh-CN.md) for full configuration and extension APIs. This page was checked against source; no paid service or physical voice acceptance was performed on your behalf.
