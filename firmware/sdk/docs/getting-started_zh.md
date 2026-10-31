[English](getting-started.md) | 简体中文

# 嵌入式 C SDK：安全上手（开发预览）

自己编写 ESP32-S3 `app_main`，调用 `watcher_hw_*`。不需要官方应用、Python SDK、
Daemon、服务端、SD 卡或模型；电脑上的 [Python Application 开发](../../../docs/sdk_zh.md)
是另一条路线。当前不是稳定发布，完整配对实机验收未完成。

## 下载测试固件（开发预览）

使用[硬件体检开发预览 Release](https://github.com/orulink-ai/WatcheRobot-public/releases/tag/hardware-check-preview-20261030)，
不是官方整机稳定版。附件不可见或无法下载时，先联系维护者，不能替换成同名相似文件。
二进制来源、源码提交、哈希和资产操作方式**跟随该 Release**，
不能默认二进制来源等于本快照上游 SHA。下载包要求 **Python 3.10+** 与说明中的 esptool 依赖。
应用单独 `.bin` **不能**交给网页烧录器从 `0x0` 写入；必须使用完整生成的烧录文件集、
准确偏移及已验证的包入口，不能猜偏移。

## 1. 获取并固定公开版本

安装 Git 与 [ESP-IDF v6.0.2](https://docs.espressif.com/projects/esp-idf/zh_CN/v6.0.2/esp32s3/get-started/index.html)
及 ESP32-S3 工具。**每个新终端都要激活 IDF 环境**：Linux/macOS 执行
`. /实际安装路径/esp-idf/export.sh`，Windows 使用 ESP-IDF PowerShell 终端。
`idf.py --version` 应显示 6.0.2；其他版本会被主动拒绝。

```sh
git clone https://github.com/orulink-ai/WatcheRobot-public.git
cd WatcheRobot-public
git checkout --detach <PUBLIC_COMMIT>
idf.py -C firmware/sdk/examples/hardware_check build
```

把 `<PUBLIC_COMMIT>` 替换为 GitHub 上已审查的**公开 PR head／合并提交完整 SHA**，
不是内部 ESP32 SHA。合并前先执行 `git fetch origin pull/<PR_NUMBER>/head`，再执行
`git checkout --detach FETCH_HEAD`；所有占位符必须替换。不需要私库权限或 Python 子模块。
保存 `git rev-parse HEAD`、`idf.py --version` 和 `firmware/sdk/SOURCE.json`。
该清单固定上游来源 `e23bee2a642f6c90753b370cb18c46815e0c8f58`，记录逐文件哈希及公开路径适配。
公开提交才标识这份交付文档；内部 SHA 仅用于来源追溯，不是要求读者检出的公开版本。

**后续命令统一在 WatcheRobot-public 仓库根目录、已激活 IDF 的终端执行。**
预期 build 成功，生成 `firmware/sdk/examples/hardware_check/build/watcher_hw_hardware_check.bin`
以及 IDF 烧录参数、bootloader、分区表。构建成功不代表任何机械／声学验收。

## 2. 选择示例与开发位置

| 目标 | 工程名 | 应用入口 |
| --- | --- | --- |
| 完整引导体检 | `hardware_check` | `firmware/sdk/examples/hardware_check/main/app_main.c` |
| 仅身体 | `body` | `firmware/sdk/examples/body/main/app_main.c` |
| 摄像头／音频／屏幕 | `head` | `firmware/sdk/examples/head/main/app_main.c` |
| 自定义组合应用 | `robot` | `firmware/sdk/examples/robot/main/app_main.c` |

例如 `idf.py -C firmware/sdk/examples/robot build`，替换最后一个工程名即可。
目标 ESP32-S3、16 MB flash；头部工程需要 octal PSRAM。每个工程独立维护分区、配置与
精确版本组件清单，注册表下载仍需网络。共享 `sdk.cmake` 只选硬件依赖，头部音频包含
本地 `media_lib_sal`，body 不引入它。修改应用代码，不启动官方固件顶层工程。

独立消费工程可以在 `project()` 前设置 `HW_BODY`／`HW_HEAD` 并 include 固定公开 checkout
中的 `firmware/sdk/examples/sdk.cmake`，在 `project()` 后调用 `watcher_hw_check_idf()`；
main 声明所需 `watcher_hw_*`，沿用对应 manifest、defaults 和 partitions。
必须保留完整固定 SDK，不能只复制头文件或跟随浮动 main。

## 3. 首次准备配套固件：不确定就停在烧录之前

记录板卡版本、STM32 固件版本／commit、Himax PTL 镜像 SHA256 和恢复包哈希。
[历史验收](../VALIDATION.md)不等于当前源码配对验收。STM32 必须声明 motion／LED／touch
能力；摄像头必须使用匹配的 **PTL** Himax 固件。公开产品 Releases **不保证**已有 SDK
验证过的配对组合。缺少可信兼容记录或可用恢复包，先联系维护者，**不要烧录猜测的镜像**。
HELLO、文件名、Latest 标签或 build 成功均不证明兼容。

首次准备和恢复只能依据[公开官方烧录指南](../../../docs/flashing_zh.md)与确认过的整套镜像；
它不是日常保留自定义应用的烧录入口。本预览没有提供未经验证的 STM32/Himax 替换镜像，
也不要求运行内部 workspace 命令或访问私库。

## 官方存储覆盖警告

ESP32 接口是 **SERIAL-B**（仍须核对实际枚举端口）。首次完整自定义 `flash` 会覆盖
官方 `agentcfg` **0x12000–0x20000** 区域，不只是可能丢数据。继续前必须备份所需配置／数据。
返回官方固件必须执行**完整官方恢复**，不能只刷 app bin 或 `app-flash`；之后重新验证启动和配对。
仅有备份并不等于已经恢复官方分区布局。

## 4. 日常自定义 ESP32 烧录

先备份所需数据、清空**完整行程**、确保操作者可随时人工断电。从设备管理器或
`/dev/serial/by-id/` 核实 **ESP32** 端口，不要误用 STM32/Himax 端口；关闭其他串口监视器。
将 `<PORT>` 替换为本机真实端口（如已确认的 `COM7` 或 `/dev/ttyACM0`）。

```sh
idf.py -C firmware/sdk/examples/hardware_check -p <PORT> flash
idf.py -C firmware/sdk/examples/hardware_check -p <PORT> monitor
```

预期 esptool 写入校验成功；屏幕显示 **Hardware check**，启动日志包含
`Hardware check v2 | curved zones`。使用 **Ctrl+]** 退出 monitor。
仅写自定义 ESP32 bootloader、分区表与应用，不刷 STM32/Himax、不构建官方 app。
分区变化即使不整片擦除，也可能覆盖官方存储。**不要执行 erase-flash**。
只有确认**连接设备**已有相同自定义分区布局时，才可用
`idf.py -C firmware/sdk/examples/hardware_check -p <PORT> app-flash` 日常更新应用。
本地分区文件相同不足以证明设备分区相同；布局未知或中途刷过官方固件，应使用完整自定义 `flash`。

两个便捷入口语义一致（仍在相同根目录和 IDF 环境）：

```sh
bash firmware/sdk/examples/hardware-sdk.sh hardware_check build
bash firmware/sdk/examples/hardware-sdk.sh hardware_check flash <PORT>
```

```powershell
./firmware/sdk/examples/hardware-sdk.ps1 hardware_check build
./firmware/sdk/examples/hardware-sdk.ps1 hardware_check flash <PORT>
```

参数为 body/head/robot/hardware_check、build/flash/monitor、端口；flash/monitor 必须显式端口。
参数或环境错误退出 2，合法调用保留 IDF 退出码。不扫描端口、不暗中配对烧录或擦除。

## 5. 体检、安全、恢复与迁移

按 [26 项体检指南](hardware-check_zh.md)操作。当前舵机**没有物理位置反馈**，
命令坐标、ACK/DONE 都是软件证据；`watcher_hw_body_request_position()` 恒返回
`ESP_ERR_NOT_SUPPORTED`。旧反馈相对运动逻辑已停用。安全确认后可能先命令 X90/Y120，
实际起点未知，**参考移动也可能是大行程**，不能只为三度腾空间；详见[开环合同](servo-open-loop_zh.md)。

断连后 ESP32 无法保证停止命令到达，必须依赖 STM32 安全机制及人工断电条件。
身体事件溢出后 FAULT 锁定，取消待执行动作；回调／分发外显式 close/init、重新订阅，
等待新的 READY 与新用户操作。重连或排空队列不能解锁。JPEG 回调数据要复制后留用；
LVGL 访问需加锁；阻塞音频 read/write/drain 由同一 owner 任务执行，不可在 ISR、MCU／
相机回调或 LVGL 锁内调用。

恢复官方固件时关闭自定义程序及串口监视器，选定已确认的**完整官方 Release**，
按[公开完整恢复流程](../../../docs/flashing_zh.md#full)核对实际端口和镜像。
恢复会替换自定义 ESP32 app，也可能替换存储与配对固件。恢复后检查官方启动、配网／配对；
返回 SDK 开发则重新 build／flash 自定义工程。恢复包缺失或来源未知时停止，不能猜偏移地址。

旧工程需将 `watche_hw_*` 头文件／函数／类型／CMake 依赖改为 `watcher_hw_*`，
常量改为 `WATCHER_HW_*`，IDF 检查改为 `watcher_hw_check_idf()`；没有兼容别名。
第一次执行 `idf.py -C firmware/sdk/examples/robot -B build-watcher build` 避开旧缓存，
不要给旧固件改名冒充新版本。[迁移指南](../MIGRATION.md)还包含丢事件合同变化。
历史记录保留当时名称与哈希，不能当作改名后新验收。

## 排障与验收边界

- IDF 版本错误：当前终端重新激活 6.0.2，不绕过版本检查。
- 缺组件：确认完整固定 checkout、注册表网络与 manifest，不 include 官方 firmware/s3。
- 身体非 READY：检查供电／连接／版本能力／静默超时，不强行运动。
- 相机失败：先查 PTL 配套，再观察超时与重试；没有图片不能通过。
- 无声：Test speaker 分别试 voice／tone，默认输出 90，上限 95；进度完成不等于可听。
- 体检结果只在 RAM，重启清空；人工结论另行记录。

软件／host／站点构建不证明机械限位、停止时延、边缘触摸、图像／声音质量、资源反复释放
或长期组合稳定性。完整配对实机与官方恢复验收仍未完成，见[验收边界](../VALIDATION.md)。
本工作不合并、不更新生产、不烧实体设备、不发布稳定 Release。
