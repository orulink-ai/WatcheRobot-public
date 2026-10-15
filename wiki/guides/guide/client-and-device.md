# 客户端与设备

Watcher Desktop 是用户操作入口，但它不直接复制一套设备通信实现。客户端监管 SDK Daemon，Daemon 统一拥有设备连接并管理当前 Application。

```text
Watcher Desktop
    ↓
SDK Daemon
    ├─↔ WatcheRobot 设备
    └─↔ 当前 Application
```

## 连接状态

| 状态               | 含义                             | 建议操作                       |
| ------------------ | -------------------------------- | ------------------------------ |
| 未发现设备         | 客户端尚未在可用链路上识别机器人 | 检查供电、网络和本机防火墙     |
| 等待配对           | 已发现设备，但尚未建立可信关系   | 在机器人上查看配对码并完成配对 |
| 已配对、未连接     | 已保存配对关系，当前链路不可用   | 确认电脑和设备位于同一局域网   |
| 已连接             | Daemon 已建立设备链路            | 可以启动兼容的 Application     |
| Application 未运行 | 设备在线，但当前业务程序未启动   | 从客户端选择并启动 Application |

## 什么时候重新配网

只有以下情况需要重新配置 Wi-Fi：

- 路由器或 Wi-Fi 密码发生变化；
- 设备保存了错误凭据；
- 设备被带到新的网络环境；
- 用户主动执行了“清除设备数据”。

客户端连接短暂中断不等于凭据损坏。先等待自动重连，再检查局域网，最后才考虑重新配网。

## Application 运行时的消息路径

当 Application 运行时，桌面业务消息会先进入当前 Application，再由 Application 决定如何与设备交互。没有 Application 运行时，Daemon 才提供透明的桌面与设备转发。

这个边界保证第三方 Application 能明确拥有自己的协议，同时避免客户端建立第二条设备业务连接。

## 下一步

- [固件版本管理](./firmware-version-management.md)
- [Python SDK 概览](../sdk/index.md)
- [故障排查](../troubleshooting/index.md)
