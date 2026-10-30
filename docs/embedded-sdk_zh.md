[English](embedded-sdk.md) | 简体中文

# 嵌入式 C SDK（开发预览）

## 先选开发路线

| 路线 | 你的代码运行在哪里 | 入口 |
| --- | --- | --- |
| Python SDK | 电脑端 Application，使用现有固件及 Runtime／Daemon | [Python 快速开始](sdk_zh.md) |
| 嵌入式 C SDK | ESP32-S3，自有 `app_main`、UI 与硬件策略 | [公开源码与上手](../firmware/sdk/README_zh.md) |

两条路线独立，不是互相安装的前提。嵌入式示例不需要 Python SDK、官方应用、Daemon、
服务端、SD 卡或模型。

## 第一步怎么走

1. [固定公开 commit，安装 ESP-IDF 6.0.2](../firmware/sdk/docs/getting-started_zh.md)。
2. 构建 `hardware_check`；烧录前核对配套固件、恢复镜像和 ESP32 实际端口。
   所有示例命令在**公开仓库根目录**执行。
3. [按 8 组／26 细项体检](../firmware/sdk/docs/hardware-check_zh.md)，再修改 body／head／robot
   应用入口，开发自己的产品。

公开快照包括 `watcher_hw_{core,body,camera,audio,display}`、必要纯硬件依赖、公共 UI／
状态机、许可测试语音、四个独立工程及 Shell／PowerShell 入口。头文件声明所有权和
线程合同；[迁移指南](../firmware/sdk/MIGRATION.md)覆盖旧命名。
完整固定上游 SHA 与逐文件来源见 [SOURCE.json](../firmware/sdk/SOURCE.json)，不要求私库访问。

## 当前硬件与发布边界

当前舵机**没有物理位置反馈**。位置查询返回 `ESP_ERR_NOT_SUPPORTED`；命令坐标、
ACK/DONE 都不是测量。运动测试必须先人工确认完整行程安全，再命令 X90/Y120 参考姿态；
未知起点可能使参考动作成为大行程。STOP 仍要观察实际停止并依赖 STM32 安全机制。
[运动前必读](../firmware/sdk/docs/hardware-check_zh.md)。

这是**开发预览**，不是稳定 Release、完整 STM32/Himax 配对验收、工厂认证或机械安全保证。
相机要求 PTL Himax 固件；不能假定当前公开官方 Release 已兼容。缺少验证过的配对／恢复镜像，
就阻塞在烧录之前。[首次准备、日常自定义烧录、恢复官方](../firmware/sdk/docs/getting-started_zh.md)
是三种不同流程。

[历史记录](../firmware/sdk/VALIDATION.md)保留当时的名称、哈希和限制；
[本次软件同步证据](../firmware/sdk/SYNC_VALIDATION.md)不构成新声音／图像／机械实测。
本次源码与文档 PR 不发布稳定 v1，不修改生产。
