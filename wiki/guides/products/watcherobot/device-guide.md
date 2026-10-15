[English](../../en/products/watcherobot/device-guide.md) | 简体中文

# 设备使用指南

从开机、扫码配网，到连接电脑和手机。步骤按 2026-10-15 本地 ESP32 源码 `7699527` 核对；截图来自已有设备指南，界面细节可能随固件和 App 版本变化，按钮含义以本文为准。

## 开关机与基本操作 {#controls}

长按设备侧面的旋钮约 **3 秒**开机或关机。

![长按侧面旋钮约 3 秒开机或关机](../../../../docs/images/device/device-guide/power.webp)

滑动滚轮选择模式；进入模式后，从屏幕两侧向中间滑动可退出。

<div class="device-guide-grid device-guide-controls">

![滑动滚轮选择设备模式](../../../../docs/images/device/device-guide/mode.webp)

![从屏幕边缘向中间滑动退出模式](../../../../docs/images/device/device-guide/exit.webp)

</div>

## 安装客户端 {#install-app}

[前往开源仓库 Releases 下载手机 App](https://github.com/orulink-ai/WatcheRobot-public/releases)

展开包含手机 App 的发布条目下的 **Assets**，下载 `watcher-android-*.apk`，传到 Android 手机安装。仅包含固件的发布条目不提供手机 App；请查找包含 APK 的应用或整套发布，不要下载固件 ZIP。iOS 使用 [TestFlight](https://testflight.apple.com/join/XFCFsm5M)，是否可加入以 TestFlight 当前显示为准。

电脑用户也从 Releases 下载 Watcher Desktop：Windows 选择适合本机的 `.exe` 安装程序，macOS 选择与芯片架构匹配的 `.dmg`。以附件实际提供的平台为准，不将 APK、固件 ZIP 或 SDK wheel 当作桌面安装包。

## 扫码配置 Wi-Fi {#wifi}

### 1. 在机器人上打开配网二维码

首次启动且没有已保存的 Wi-Fi 时，设备会自动打开配网页。也可进入 **Settings → Wi-Fi**；等待蓝牙广播就绪后显示 **Scan QR** 和二维码。

打开手机蓝牙并允许 App 使用蓝牙和相机。请用 **WatcheRobot App 内扫码**，不是微信或系统相机；二维码仅用于识别目标设备，不包含 Wi-Fi 密码。设备已联网时显示网络信息而非二维码。只有需要更换网络时才点 **Disconnect**：它会清除已保存的 Wi-Fi。

<div class="device-guide-grid">

![设备上的 Settings 入口](../../../../docs/images/device/device-guide/settings.webp)

![在 Settings 中选择 Wi-Fi](../../../../docs/images/device/device-guide/wifi.webp)

![机器人显示配网二维码](../../../../docs/images/device/device-guide/qr.webp)

</div>

### 2. 用手机 App 扫码

打开 App，点击 **Scan QR code**，扫描机器人屏幕上的二维码，等待搜索设备。

<div class="device-guide-grid">

![App 首页的 Scan QR code 按钮](../../../../docs/images/device/device-guide/scan.webp)

![App 扫描二维码并搜索设备](../../../../docs/images/device/device-guide/scanning.webp)

</div>

### 3. 选择网络并连接

选择要连接的 Wi-Fi；也可以点击 **Enter network manually** 手动输入网络名称。输入 Wi-Fi 密码，等待 App 显示 **Connected successfully**，并确认设备显示 **Connected** 和 IP 地址。错误密码或连接超时不能当作成功；按 App 的错误提示重试。

扫码配网不等于进入手机控制模式。成功通知完成且 App 释放蓝牙后，设备可返回主界面；手动打开的 Wi-Fi 信息页不保证自动退出。

<div class="device-guide-grid">

![选择 Wi-Fi 或手动输入网络名称](../../../../docs/images/device/device-guide/network.webp)

![App 显示机器人已成功连接 Wi-Fi](../../../../docs/images/device/device-guide/connected.webp)

</div>

::: tip
机器人已连入正确 Wi-Fi 时，无需重新配网。连接电脑前，请让电脑和机器人处于同一局域网。
:::

SDK 也提供配网方式，见本站 [Python SDK 快速开始](../../../../docs/sdk_zh.md)。手机扫码配网不需要先安装 SDK。

## 连接电脑与 SDK {#desktop}

1. 确认电脑和机器人处于同一局域网。
2. 在机器人上进入 **Desktop Link**。
3. 查看屏幕上的 **PAIR CODE** 六位配对码。

<div class="device-guide-grid">

![选择 Desktop Link 模式](../../../../docs/images/device/device-guide/desktop.webp)

![Desktop Link 显示六位配对码，等待电脑连接](../../../../docs/images/device/device-guide/pair.webp)

</div>

使用 Watcher Desktop 时，按客户端提示输入配对码。使用 SDK 时，先按 [Python SDK 快速开始](../../../../docs/sdk_zh.md)完成安装，再在终端执行：

```bash
watcherobot robot pair 123456
```

将 `123456` 替换为设备当前显示的配对码，不要照抄截图中的号码。再运行 `watcherobot robot status` 确认设备已连接；保存配对关系不等于设备当前在线。

更多连接状态说明见[客户端与设备](../../guide/client-and-device.md)。

## 连接手机 {#phone}

1. 在机器人上进入 **Phone Control**。
2. 查看设备屏幕上的 **Device ID**。
3. 打开手机 App，点击 **Add Device**，选择与屏幕 Device ID 一致的设备，按 App 提示完成连接。

<div class="device-guide-grid">

![选择 Phone Control 模式](../../../../docs/images/device/device-guide/phone.webp)

![Phone Control 显示待连接设备的 Device ID](../../../../docs/images/device/device-guide/phone-id.webp)

![App 首页的 Add Device 按钮](../../../../docs/images/device/device-guide/add-device.webp)

</div>

**Scan QR code** 用于配置 Wi-Fi，**Add Device** 用于连接手机控制模式，两者不是同一个操作。

连接遇到问题时，查看[故障排查](../../troubleshooting/index.md)。

需要更新固件？查看[烧录指南](../../../../docs/flashing_zh.md)。欢迎[加入微信社区](community.md)，交流使用和开发经验。
