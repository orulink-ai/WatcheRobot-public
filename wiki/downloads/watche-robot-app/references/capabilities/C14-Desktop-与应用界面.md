# C14 Desktop 与应用界面

先读 [共同前提 C00](../机器人能力.md)。

**如何选择：** 对接 Watcher Desktop 业务消息使用 app.desktop；用户自己的网页或原生窗口调用应用业务函数，不把网页当作 SDK Desktop 通道客户端。

**使用前检查：** app.desktop.send/receive 是异步方法，数据类型为 str 或 bytes。业务协议由应用与前端约定，SDK 不自动创建页面或按钮。

### 处理一条自定义 Desktop 业务消息（异步入口）

```python
import asyncio
import json
from watcherobot.application import ApplicationContext


async def main():
    async with ApplicationContext.from_environment() as app:
        while not app.shutdown_requested:
            try:
                frame = await app.desktop.receive(timeout=0.5)  # 秒，定期检查退出请求
            except (TimeoutError, asyncio.TimeoutError):
                continue
            if not isinstance(frame, str):
                continue  # 这个示例协议只处理 JSON 文本
            try:
                message = json.loads(frame)
            except (ValueError, TypeError):
                continue
            if not isinstance(message, dict) or message.get("type") != "my_app.ping":
                continue  # 安全忽略其他业务协议；不修改 Daemon 路由
            await app.desktop.send(json.dumps({"type": "my_app.pong"}))


if __name__ == "__main__":
    asyncio.run(main())
```

**成功标准：** 前端发送 my_app.ping 并收到 my_app.pong 才证明这条协议可用。运行上面的程序不会自动出现界面。

**失败与清理：** 按钮拍照：应用 UI → 应用业务函数 → app.robot.camera → 保存结果 → UI 展示。Web 应用默认只监听 127.0.0.1，通过本地 HTTP/WebSocket 调业务；增加网络暴露须有实际需求和相应访问控制。设备消息不得从网页直连机器人。
