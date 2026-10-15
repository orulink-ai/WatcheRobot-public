# 快速开始

这条路径适合第一次使用 WatcheRobot 的用户。完成后，Watcher Desktop 能识别设备，你也可以继续体验官方 Application 或开始 SDK 开发。

## 开始前

- 给机器人稳定供电，并将它放在电脑附近。
- 首次配网需要 Windows 或 macOS 电脑提供蓝牙能力。
- 配网完成后，电脑与机器人应连接到同一个局域网。
- 不要在连接过程中同时运行多个 Watcher Desktop 或 SDK Runtime 实例。

## 1. 安装 Watcher Desktop

从 [Watcher Desktop Release Assets](https://github.com/orulink-ai/WatcheRobot-public/releases/latest) 下载与你的系统匹配的安装包并完成安装。

Watcher Desktop 负责界面和本地运行时监管；实际设备连接、配对和 Application 生命周期由随客户端安装的 SDK Daemon 管理。

## 2. 给设备配置网络

在机器人上打开 **Settings > Wi-Fi**，按照界面提示选择设备并输入 Wi-Fi 信息。配网成功后，设备会保存网络凭据并尝试连接局域网。

::: tip
如果设备已经连入正确的 Wi-Fi，不需要重复清除网络。直接进入下一步即可。
:::

## 3. 完成配对

1. 打开 Watcher Desktop。
2. 确认客户端显示的设备身份与机器人一致。
3. 在机器人上打开对应的连接入口。
4. 按客户端提示输入机器人显示的六位配对码。
5. 等待状态变为已连接。

更完整的状态解释见[客户端与设备](./client-and-device.md)。

## 4. 选择下一条路径

- 只使用机器人：在客户端中选择已安装的官方 Application。
- 管理固件：阅读[固件版本管理](./firmware-version-management.md)。
- 开发自己的体验：进入 [Python SDK 概览](../sdk/index.md)。
- 无法连接：从[故障排查](../troubleshooting/index.md)开始检查。
