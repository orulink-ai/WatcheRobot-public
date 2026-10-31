# Hardware SDK namespace migration / 硬件 SDK 命名迁移

The development preview now uses `watcher_hw_*` consistently. This is a source
API rename, not a change to the MCU protocol, hardware behavior or board branding.
No compatibility aliases or second implementation are maintained.
This statement describes the rename itself. The separate event-loss/status
corrections in [AUDIT.md](AUDIT.md) do change runtime behavior as described below.

开发预览统一使用 `watcher_hw_*`。这是源码接口改名，不改变 MCU 协议、硬件行为
或 `WatcheRobot` 项目名称。不保留两套实现或旧名称别名。

| Before / 原名称 | Now / 新名称 |
| --- | --- |
| `watche_hw_{core,body,camera,audio,display}` component | `watcher_hw_{core,body,camera,audio,display}` |
| `watche_hw_body.h` and other hardware headers | `watcher_hw_body.h` and matching module headers |
| `watche_hw_*` functions and types | `watcher_hw_*` functions and types |
| `WATCHE_HW_*` constants | `WATCHER_HW_*` constants |
| `watche_hw_check_idf()` | `watcher_hw_check_idf()` |
| `watche_hw_{body,head,robot}.bin` example output | `watcher_hw_{body,head,robot}.bin` |

## Existing applications / 已有自定义工程

1. Pin and obtain the updated SDK Git revision together with its matching examples.
   Update your `#include`, API/type/constant names and `main/CMakeLists.txt`
   component dependencies; update the top-level IDF-version check too.
2. Use a new build directory for the first build after migration, such as
   `idf.py -B build-watcher build` from your own project directory.
   Rebuild the application; do not rename an old binary or hand-edit generated
   CMake/flash files.
3. Use IDF-generated flashing arguments or the example's regular flash command.
   The example paths remain `body`, `head` and `robot`, and the developer
   scripts retain their existing names and parameters.

中文操作：更新并固定 SDK 版本，同时修改应用中的头文件、函数、类型、常量、
CMake 组件依赖及版本检查函数。首次改名后在自己的工程目录执行
`idf.py -B build-watcher build`，使用新的构建目录避开旧缓存。
不要给旧二进制改文件名冒充新固件，也不要手改生成的烧录参数。
三个示例目录、脚本名称和操作参数不变。现有 `watcher_sdk` 是依赖官方服务的
另一套接口，不属于本次硬件 SDK 改名。

## Event-loss contract update / 丢事件合同更新

Handle `WATCHER_HW_EVENT_OVERFLOW` and cancel every pending application action.
Its payload is `watcher_hw_body_event_loss_t`. Body controls remain locked after
loss, even across HELLO recovery. Do not resume from cached LINK READY or FIFO
drain; query status. Recover explicitly with close/init outside dispatch, then
resubscribe, wait for fresh READY and obtain new user intent. A best-effort stop
is not physical-stop confirmation. See the public body header and README.

已有应用还需处理新 OVERFLOW 事件，取消所有待执行业务动作。丢事件后控制保持锁定，
重新握手或排空队列不能恢复；需在分发／回调外显式关闭、初始化、重新订阅，并等新的
READY 和用户操作。不能相信旧 LINK READY，也不能把停止命令提交成功当成机械停止确认。
摄像头状态现在也报告连通设备的故障；`status()` 返回 ESP_OK 不表示设备 READY。

## Evidence boundary / 验收边界

Historical entries in [VALIDATION.md](VALIDATION.md) retain the names and hashes
of the firmware that was actually tested. They do not establish hardware
acceptance of this renamed source. Namespace checks and rebuilt examples prove
source/build consistency, not audible, visible or mechanical correctness.

[VALIDATION.md](VALIDATION.md) 的旧条目保留当时实际测试固件的名称和校验值。
改名测试、重新构建不等于重新完成声音、图像或机械运动的实机验收。
