# C12 RTC

先读 [共同前提 C00](../机器人能力.md)。

**交付等级：高级、需配套媒体客户端。** 本节仅覆盖 SDK 控制和信令，包内没有完整收流客户端。先确认已有客户端及固件媒体协议；缺少时只能交付控制代码和明确缺项，不能宣称完成实时视频应用。

### 与拍照、程序表情组合

- 配套固件支持时，音频 RTC 可配合普通 `camera.capture()` 在通话中拍照；反馈拍照会被任何活动 RTC（包括 audio）拒绝，必须先结束本应用 RTC，再释放相机、音频和显示冲突；只释放屏幕不够。video/av RTC 占用相机，不能同时拍照。
- 记录本应用启动的会话和表情，退出时只清理自己持有的资源。拍照尚未结束时，先等结果，再释放相关显示资源。
- 设备重连后不要沿用旧会话。停止失败时保留待清理状态，不能直接报告已释放。

**如何选择：** 实时视频选 video；双向音频选 audio；同时音视频选 av。此处是会话控制和信令，不是只调用 start 就得到完整浏览器通话。

**使用前检查：** 视频需 rtc.video.mjpeg.v1；音频需 rtc.audio.full_duplex.v1。媒体客户端负责实际采集、协商、解码和播放。相机及音频占用按选择的模式处理。

### 启动、查看和停止会话

```python
from app_support import device_session

import time

mode = "av"  # 改为 "video" 或 "audio" 即选择对应模式。
# 为每种媒体模式列出所需的设备能力；av 需要音频和视频两项能力。
required = {
    "video": ["rtc.video.mjpeg.v1"],
    "audio": ["rtc.audio.full_duplex.v1"],
    "av": ["rtc.video.mjpeg.v1", "rtc.audio.full_duplex.v1"],
}
if not all(robot.supports(cap) for cap in required[mode]):
    raise RuntimeError("设备不支持所选 RTC 模式")

# 发起 RTC 会话控制；媒体真正连通还需要客户端完成后面的信令交换。
with device_session(
    lambda: app.rtc.start(mode=mode),
    app.rtc.stop,
):
    # 查看当前会话状态快照。
    print(app.rtc.snapshot())
    # after：事件 ID 游标；0 表示读取当前保留的事件。
    print(app.rtc.events(after=0))
    # 观察会话状态 5 秒；这不保证媒体已连接。
    time.sleep(5)
```

### 转发媒体客户端信令

**函数片段，不会自动执行。** 只有存在实现下方协议的媒体客户端时，才能在应用 RTC 任务中调用 `forward_rtc_signaling`；此函数不是完整客户端。

```python
# 以下放在活动会话中，由真实媒体客户端提供参数。
# rtc：活动会话控制对象（如 app.rtc）；offer_sdp / candidate 由媒体客户端生成。
def forward_rtc_signaling(rtc, offer_sdp, candidate, mid, index):
    # offer_sdp：描述媒体、编解码和传输参数的 SDP offer 文本。
    rtc.send_offer(offer_sdp)
    # 转发一条 ICE 网络候选，用于协商可用的连接地址。
    rtc.send_candidate(
        # candidate：候选内容；mid：SDP 媒体段标识；index：从 0 开始的媒体行索引。
        candidate, sdp_mid=mid, sdp_mline_index=index,
    )

# 持续读取新增事件时保存最后一个事件 ID。
# 首次从游标 0 读取保留的事件；上面的函数需由真实客户端流程调用。
events = app.rtc.events(after=0)
# 保存最后的事件 ID；后续调用 events(after=last_id) 获取新增事件。
last_id = events[-1]["id"] if events else 0
# 查询当前会话快照，便于检查信令处理后的状态。
print(app.rtc.snapshot())
```

**成功标准：** 分别检查控制 ACK、信令状态和实际媒体帧/声音；必须最后一项成功才报告接通。事件 after 是递增 ID 游标，不是时间戳。

**失败与清理：** offer_sdp/candidate/mid/index 必须来自真实媒体客户端。reset(reason=...) 只清除本地状态，不能代替 stop。停止时关闭客户端 PeerConnection/媒体轨道，并 stop 本次 app.rtc。

### 媒体客户端必须完成的部分

应用提供自己的本地信令接口，把客户端真实 offer/ICE 交给 `app.rtc`；按 `events(after=last_id)` 读取新事件，将其中 `message` 的真实 answer/候选交回客户端，不制造 SDP。浏览器媒体轨道和页面资源由客户端释放。

`watcher-rtc/1` 的媒体形式取决于配套固件：历史视频方案有名为 `mjpeg-data` 的 WebRTC 数据通道；PTL 纯视频方案使用设备公告的 MJPEG 媒体端点。不能假设所有视频都是标准 WebRTC video track，也不能硬编码设备 IP/端口。协议明确公告的媒体传输不是第二条设备业务控制连接；控制始终走 Application。

本 skill 给出公开 SDK 控制调用，不内置完整 RTC 浏览器播放器。用户要完整通话页面时，必须实现并验证匹配设备公告的媒体客户端；缺少对应媒体合同或客户端实现就明确该部分未完成，不能把上面的五秒启动示例交付为通话应用。


