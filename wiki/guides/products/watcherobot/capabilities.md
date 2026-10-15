[English](../../en/products/watcherobot/capabilities.md) | 简体中文

# 机器人原子能力

原子能力是 Application 可以调用的设备基础功能。先按[Python SDK 概览](../../sdk/index.md)安装、配对，并通过 `watcherobot app run` 运行应用。

本页于 **2026-10-15** 按 Python SDK **0.1.12**（`cc0a319`）与 ESP32 源码 `7699527` 核对。公开总仓的 `python-sdk/` 固定引用可能较旧；需要本页接口时，先核对已安装 SDK 版本和设备能力。源码基线不等于已发布的配套固件组合。实际可用能力取决于当前 SDK、固件、硬件与已安装资源；不是所有设备都支持下列全部功能。

## 云台动作 {#motion}

| 编号 | 能力 | API 入口 | 使用要点 |
| --- | --- | --- | --- |
| 01 | 转到指定角度 | `robot.motion.move_to()` | 水平、俯仰和时长；返回 Job。协议角度 0–180°，实际受机械限位约束。 |
| 02 | 连续更新目标 | `robot.motion.set_target()` | 可单独更新水平或俯仰；无 Job，返回不代表到位。 |
| 03 | 播放预设动作 | `robot.motion.play_action()` | 使用设备已安装的动作 ID，不是主机文件路径。 |

需要 `motion`。结束时使用 `robot.motion.stop()`；不要与设备端人脸跟随同时控制云台。运动前确保周围安全。上述基线不保证 `linear` / `ease_in_out` 有不同执行曲线。

## 灯光与屏幕 {#display}

| 编号 | 能力 | API 入口 | 使用要点 |
| --- | --- | --- | --- |
| 04 | 灯色与亮度 | `robot.lights.set_color()` | `light`；亮度 0–1，灯区 `side` / `bottom` / `all`。 |
| 05 | 灯效 | `robot.lights.play_effect()` | `light`；闪烁、呼吸、彩虹、状态脉冲；`repeat=0` 持续播放。 |
| 06 | 屏幕动画 | `robot.animation.available_ids`、`robot.animation.play()` | `animation`；从设备公告的资源中选择，循环动画需主动停止。 |
| 07 | 代码表情 | `robot.expression_runtime.start()` / `update()` / `stop()` | 调整眼形、视线、颜色、眨眼和装饰；支持预设、像素和矢量方式。 |

灯效用 `robot.lights.off()` 结束。隐私指示灯由固件管理。

动画预加载需另查 `animation.prefetch.v1`，预加载不会开始播放。代码表情自定义装饰需检查 `expression.runtime.v3`，以及像素的 `expression.pixel_accessory.v1` 或矢量的 `expression.vector_accessory.v1`。像素位图为 51×51、652 个十六进制字符；矢量基于 412×412 画布。

## 声音 {#audio}

| 编号 | 能力 | API 入口 | 使用要点 |
| --- | --- | --- | --- |
| 08 | 设备音效 | `robot.audio.play()` | `audio`；参数是设备 SD 资源中的音效 ID。 |
| 09 | 主机音频文件 / PCM | `robot.audio.play_file()` / `play_pcm()` | `audio.stream`；文件由主机解码发送到设备。PCM 为 24 kHz、单声道、16-bit S16LE，最大 4 MiB。 |
| 10 | 麦克风 | `robot.microphone.open()` / `open_pcm()` / `record_pcm()` | `microphone`；分别读取 Opus 包、PCM 帧或定时录音。PCM 输出为 16 kHz、单声道、16-bit。 |

普通播放与普通麦克风互斥，不可同时开启。`play_pcm()` 一次提交完整字节，不是持续写入接口。Opus 包不能直接当 PCM 保存为 WAV；PCM 解码需要主机依赖。定时录音使用 `record_pcm()`，上述基线中的 `record()` 不支持。

## 视觉 {#vision}

| 编号 | 能力 | 能力标识 | 使用要点 |
| --- | --- | --- | --- |
| 11 | 拍照 | `camera.capture`；带反馈另需 `camera.capture.feedback.v1` | `robot.camera.capture()` / `capture_with_feedback()` 获得 JPEG；带反馈拍照同时使用相机、显示和音效，资源冲突时不会自动退化为普通拍照。 |
| 12 | 端侧视觉 | `vision.inference.v1`；预览另需 `vision.inference.preview.v1` | 查询和选择模型、推理、获取结果与画面。模型以设备目录为准。 |

使用 `robot.vision.models()` 查询实际安装的模型，通过 `robot.vision.start_inference(model_id, preview=True)` 开始推理。模型能力取决于 Himax 配套固件及 ESP32 构建配置；`verified` 不等于准确率保证。相机被跟随、推理、录制或 RTC 视频占用时，先停止原会话再切换。

## 触摸与滚轮 {#inputs}

| 编号 | 能力 | 能力标识 | 事件来源 |
| --- | --- | --- | --- |
| 13 | 背部触摸 | `input.back_touch` | `back_touch`：动作、触摸传感器 ID。 |
| 14 | 屏幕点击 | `input.screen_touch` | `screen_touch`：点击坐标。 |
| 15 | 滚轮旋转 | `input.roller` | `roller`：有符号步进增量，不是绝对角度。 |

统一使用 `robot.inputs.wait(timeout=...)` 读取，并按 `event.source` 区分来源。输入共用队列；`clear()` 会丢弃积压事件，`dropped_events` 是队列丢弃计数。

## 常用组合能力 {#composed}

这些功能组合多个设备资源，不应当作可任意并行的独立动作。

| 编号 | 能力 | API 入口 / 能力标识 | 边界 |
| --- | --- | --- | --- |
| 16 | 人脸跟随 | `robot.face_tracking.start()` / `stop()`；`face_tracking.control.v1` | 占用相机和云台；停止可选择保持位置或回中。 |
| 17 | 人脸跟随预览 | `robot.face_tracking.open_preview()`；`face_tracking.preview.v1` | 读取画面、人脸框和跟踪状态；退出上下文关闭预览并停止跟随。 |
| 18 | 录音录像 | `robot.recordings.start()` / `start_host()` | `recording.device.v1` / `recording.host.v1`；音视频需核对 `recording.device.av.v1`，屏幕预览需 `recording.screen_preview.v1`。 |
| 19 | RTC | `app.rtc.start()` / `stop()` | 视频需 `rtc.video.mjpeg.v1`，全双工音频需 `rtc.audio.full_duplex.v1`；音视频需要两项。 |
| 20 | 行为、完整作品、表情片段 | `robot.behavior.play()`、`robot.works.play()` / `play_expression()` | 分别检查 `behavior`、`resource.work.play`、`resource.work.expression.play`，使用已安装资源 ID。 |

录制下载得到 manifest 和 WREC 分段，转换为 MP4 / WAV 在主机进行。主机录制 `close()` 只关闭本地接收，不能代替设备 `stop()`；提前结束应停止、读到结束帧再关闭。长时主机录制需要每 5 秒心跳，设备不限时录制也需要心跳。请求帧率不是实际帧率保证。

RTC 的 `start()` 只启动会话控制，媒体客户端仍需完成真实 SDP/ICE 信令；不能仅凭调用成功认为已建立通话。`reset()` 只清理本地状态，不能代替 `stop()`。

作品播放只返回命令确认，不提供 Job。ASR、LLM、TTS 和 AI 对话由 Application 组合模型或服务实现，不属于这些硬件原子能力。

## 在 Application 中查询能力 {#inspect}

下面的示例只查询，不驱动硬件。将它作为已初始化 Application 的 `app.py`，用 `watcherobot app run` 运行，不要直接执行 `python app.py`。

```python
import asyncio
from watcherobot.application import ApplicationContext


def inspect(app):
    robot = app.robot
    print(robot.capabilities)
    print("Motion supported:", robot.supports("motion"))
    print("Camera supported:", robot.supports("camera.capture"))


async def main():
    async with ApplicationContext.from_environment() as app:
        await asyncio.to_thread(inspect, app)


asyncio.run(main())
```

同步设备调用放入工作线程，避免阻塞异步消息处理。ACK 只表示命令被接受；有 Job 的操作需等待终态。`Job.wait()` 超时不会自动停止设备，取消请求也不是已停止的证明。媒体会话结束时使用对应的 `stop()` / `close()`。

接口清单来自文档与源码静态核对，不代表本页完成了所有能力的真机验证。遇到问题可查看[故障排查](../../troubleshooting/index.md)或[加入微信社区](community.md)。
