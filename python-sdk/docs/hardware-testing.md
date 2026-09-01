# Hardware testing

`tools/hardware_smoke.py` 是 WatcheRobot Python SDK 的维护者台架工具，不是用户 quickstart。它可以顺序验证
灯光、动画、主机 WAV、Behavior、动作、摄像头和麦克风，并把媒体结果统一写入 `artifacts/hardware-smoke/`。

## 人工交互模式

使用机器人屏幕上的配对码：

```bash
python -m pip install -e ".[hardware]"
python tools/hardware_smoke.py --pairing-code 123456 --all
```

脚本会在每项执行后等待确认；输入 `f` 可把当前项目标记为失败。

## 自动台架模式

如果维护者的台架固件启用了调试 CLI，可以通过串口临时打开 SDK Control App 并读取测试配对码：

```bash
python tools/hardware_smoke.py --auto-pair-port COM5 --all --non-interactive
```

该能力仅用于维护者本地台架。公开 Release 固件不应依赖串口输出配对码。

## 输出与隐私

- 默认输出目录：`artifacts/hardware-smoke/`
- 摄像头文件：`camera.jpg`
- 麦克风文件：`microphone.wav`
- `artifacts/` 整体被 Git 忽略

拍照和录音前应确认现场人员知情。测试结束后由操作者决定保留或删除本地产物。

## 发布验收记录

每次 TestPyPI/PyPI 发布前，在 PR 或 Release 说明中记录以下信息：

| 字段 | 要求 |
|---|---|
| Python SDK | 完整版本，例如 `0.1.0a4` |
| SDK protocol | 当前协议版本，例如 `1.0` |
| ESP32 固件包 | 真机烧录的 Release ZIP 文件名和 SHA256 |
| ESP32 firmware | 设备上报的固件版本 |
| 测试命令 | 完整 `hardware_smoke.py` 命令，隐去真实配对码 |
| 测试结果 | 各能力通过/失败，以及人工确认结论 |
| 媒体检查 | `camera.jpg` 可解码、`microphone.wav` 可播放且格式正确 |

核心能力失败时停止发布。测试用协议假机器人只能作为自动化回归，不得代替这份真机记录。
