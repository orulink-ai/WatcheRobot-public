# 用树莓派建设真机 CI 测试台

WatcheRobot 同时包含 ESP32-S3 和 STM32F103。普通 CI 可以证明代码能够编译，Hardware-in-the-loop（HIL）则把固件烧录到真实机器人，通过启动日志、双 MCU 通信和外部动作判断功能是否正常。

> [!IMPORTANT]
> HIL 测试台正在建设中。本页描述已经确定的架构、当前 Release 基线和分阶段推进方式，不代表所有功能测试已经上线。

## 为什么使用树莓派

树莓派通过 USB 连接 ST-LINK、ESP32 调试接口和串口桥接器，可以长期作为固定的硬件执行节点。它不需要桌面或串口监视窗口：测试程序直接采集字节流，保存原始日志并生成 CI 报告。

```text
GitHub 托管 Runner
  └─ 编译固件并生成 Artifact
                 │
                 ▼
树莓派 HIL 节点
  ├─ 烧录 ESP32
  ├─ 烧录 STM32
  ├─ 采集双路串口
  ├─ 控制复位和电源
  └─ 输出 PASS / FAIL
```

构建与真机执行分开后，树莓派不需要为每个 PR 完整编译 ESP-IDF，并且静态检查和真机检查能够使用同一份二进制。

## 当前 Release 基线

第一阶段以正式发布资产验证测试台：

| 控制器 | Release | 当前验证边界 |
|---|---|---|
| ESP32-S3 | `v0.4.2` | 下载、完整性校验、烧录和启动测试 |
| STM32F103 | `v0.1.2` | 发布包尚未完成 ST-LINK 真机验收，HIL 将补齐该环节 |

基线版本由 `WatcheRobot_hil` 仓库显式记录。系统不会在无人评审时自动漂移到新的 Latest Release。

## 代码存放在哪里

HIL 程序保存在独立的 `WatcheRobot_hil` GitHub 仓库，并作为子仓库加入 `WatcheRobot-Workspace`。GitHub 是唯一可信源码；树莓派上的目录只是一个可以重新部署的工作副本。

开发阶段使用普通目录：

```text
~/projects/WatcheRobot_hil/
├─ src/                 测试控制程序
├─ tests/               不依赖硬件的自动化测试
├─ config/              Release 基线和测试台配置模板
├─ firmware-cache/      下载的固件，不提交 Git
└─ logs/                真机日志，不提交 Git
```

测试台稳定后，正式运行版本会安装到 `/opt/watcher-hil`，配置、运行状态和日志再分别放入系统目录。这样开发中的半成品不会影响正在执行的 CI。

## 从 Release 到 PR 检查

当前没有待验证 PR 时，测试台下载 ESP32 与 STM32 正式 Release，校验 SHA-256 后执行基础测试。以后固件 PR 会先在 GitHub 托管 Runner 上构建精确 commit，再把 Artifact 交给同一个 HIL Harness。

```text
现在：Release → 下载 → 校验 → 烧录 → 真机测试
以后：PR commit → 构建 Artifact → 烧录 → 真机测试 → 回写 PR
```

## 分阶段验收

1. 环境自检、Release 下载和校验能够重复执行。
2. 固定识别 ESP32、STM32 串口和 ST-LINK，完成自动烧录。
3. 同步采集双路日志，完成启动和通信 Smoke Test。
4. 接入电源控制、电机和编码器闭环测试。
5. 连续运行稳定后，先作为非阻塞 PR 检查，再升级为 Required Check。

在测试台能够稳定区分“产品失败”和“测试基础设施失败”之前，不应阻断正常 PR 合并。

