[English](../en/guide/getting-started.md) | 简体中文

# 快速开始

本页面向已有可用整机的用户，从开机和配网开始，不需要先下载源码或重新烧录。想自己动手制作，请看[复现指引](reproduction.md)。

## 1. 开机并安装客户端

稳定供电，在安全、平整的位置开机。手机 App 和桌面端下载入口见[设备使用指南](../products/watcherobot/device-guide.md#install-app)，按你的系统选择安装包。

## 2. 扫码配置 Wi-Fi

第一次开机没有已保存的网络时，设备会自动打开配网页；也可进入 **Settings → Wi-Fi**。打开手机 App 的 **Scan QR code**，扫描机器人屏幕二维码，选择 Wi-Fi 并输入密码，等待手机提示成功和设备显示 Connected/IP。

完整图文见[扫码配网](../products/watcherobot/device-guide.md#wifi)。手机需要蓝牙与相机权限；不要求电脑有蓝牙。已连接正确网络时不要反复点击 Disconnect，它会清除已保存的 Wi-Fi。

## 3. 选择连接方式

| 你要做什么 | 设备入口 | 下一步 |
| --- | --- | --- |
| 使用电脑客户端 | Desktop Link | 电脑与机器人同一局域网，按客户端提示输入设备六位配对码 |
| 用手机控制 | Phone Control | App 点击 Add Device，核对 Device ID 后连接 |
| 开发自己的应用 | Desktop Link | 按 [SDK 快速开始](../../../docs/sdk_zh.md)配对并运行 Application |

配对成功不等于应用已运行。连接状态见[客户端与设备](client-and-device.md)。

## 4. 检查并继续

先在安全范围内检查一个表情、灯效和小幅动作，见[首次运行检查](../../../docs/action-test_zh.md)。使用电脑时，在客户端选择并启动已安装的官方或自定义 Application。

- 更新固件：[烧录指南](../../../docs/flashing_zh.md)与[版本管理](firmware-version-management.md)。
- 编写应用：[原子能力](../products/watcherobot/capabilities.md)与[语音对话](../products/watcherobot/voice.md)。
- 连接异常：[故障排查](../troubleshooting/index.md)；需要交流时加入[微信社区](../products/watcherobot/community.md)。
