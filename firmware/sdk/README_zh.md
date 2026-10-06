[English](README.md) | 简体中文

# ESP32 嵌入式硬件 SDK（开发预览）

这里提供 `watche_hw_*` 源码、必要驱动和三个独立 ESP-IDF 工程。
开发者拥有自己的 `app_main`，不需要官方应用、Python SDK、Daemon、服务端、
SD 卡或模型。尚未完成全部实机验收，不是稳定版发布。

## 小白从这里开始

1. 下载本仓库，或执行 `git clone https://github.com/orulink-ai/WatcheRobot-public.git`。
2. 安装 ESP-IDF **6.0.2** 和 ESP32-S3 工具链，在 ESP-IDF 终端执行后续命令。
3. 打开 `firmware/sdk/examples/robot/main/app_main.c`，这是你自己的应用。
4. 在仓库根目录执行 `idf.py -C firmware/sdk/examples/robot build`。
5. 确认设备端口和配套固件，备份需要的数据，再执行
   `idf.py -C firmware/sdk/examples/robot -p <PORT> flash monitor`。

`<PORT>` 替换为实际串口名称，不要原样输入。工程目标为 ESP32-S3、16MB Flash，
头部示例启用 octal PSRAM。构建不等于实机安全验收。身体示例长按可能触发小幅运动，
首次操作前保持机构运动空间畅通。

## 示例与模块

- `examples/body`：运动、灯光、身体触摸。未收到新鲜有效位置反馈不运动。
- `examples/head`：LVGL 页面和屏幕触摸、JPEG 拍照、短时 PCM 录放音。
- `examples/robot`：上述能力组合，建议作为开发起点。
- `components/sdk/watche_hw_*`：公共接口；`components` 其余目录是必要底层依赖。

全部示例有自己的 CMake、分区和配置，不引入官方固件顶层工程。
公共头文件说明参数、所有权和线程限制。身体回调由应用调用 `dispatch_events`
分发，运动提交成功不是运动完成；未就绪拒绝控制，重连不重放动作。
摄像头回调 JPEG 只在回调内有效，需保存则复制；音频阻塞操作放应用任务；
LVGL 操作使用显示锁，不能在持锁时录音或关闭显示。

## 在自己的新工程中使用

将本仓库的固定版本作为依赖保留在项目旁边。自己的顶层 CMake 在
`project()` 前设置 `HW_BODY` / `HW_HEAD`，并 include 本 SDK 的
`examples/sdk.cmake`；它使用自己的位置寻找组件，不依赖私有仓库。
在 `project()` 后调用 `watche_hw_check_idf()`。主组件声明实际使用的
`watche_hw_*` 依赖，并沿用相应示例的依赖清单、分区和 sdkconfig.defaults。
只需身体时关闭 `HW_HEAD`，不会初始化头部。不要依赖浮动 main 发布产品。

## 脚本与烧录边界

Windows：`./firmware/sdk/examples/hardware-sdk.ps1 robot build`。
macOS/Linux：`bash firmware/sdk/examples/hardware-sdk.sh robot build`。
两个入口的参数都是：示例（默认 robot）、操作（默认 build）、端口。
操作支持 build/flash/monitor；flash 和 monitor 必须显式指定端口。

首次准备 STM32/Himax 和恢复官方整机见[官方烧录指南](../../docs/flashing_zh.md)。
现有官方工具会替换自定义 ESP32 应用，不要用于日常 SDK 烧录。
SDK 摄像头需要 PTL 配对 Himax；现有 Release 不保证包含该配对，
未经[兼容验收](VALIDATION.md)确认不要直接刷写未知镜像。
自定义首次 flash 会写 bootloader 和分区表，可能覆盖存储；不要执行 erase-flash。
确认分区不变后日常可用 `app-flash`。断线停止依赖 STM32 安全机制，
不能承诺断线后 ESP32 还能发送停止命令。

## 许可证与维护

本目录自有代码沿用内部 ESP32 源码的 [Apache-2.0](LICENSE)，
不以公开仓库根目录 GPL-3.0 替代原许可。Seeed BSP、SSCMA、I/O expander
和 Espressif codec 等保留各目录原有许可证与版权；注册表依赖也适用其各自许可。
本目录是 SDK 的对外交付快照，开发源仍在内部 ESP32 仓库；对外修复应回流开发源，
不能维护两套独立实现。官方应用、STM32/Himax 实现不在本目录公开范围内。
