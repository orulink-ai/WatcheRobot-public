# Python SDK 概览

`watcherobot` 是 WatcheRobot 的官方 Python SDK，同时包含设备 Runtime/Daemon。开发者编写的程序以 Application 身份运行，由 Daemon 注入 Desktop channel 和 Device channel。

## 环境要求

- Python 3.10–3.12，推荐 Python 3.11；
- Windows 或 macOS 用于首次蓝牙配网；
- 电脑与机器人位于同一局域网；
- 机器人保持供电并运行兼容固件。

## 安装

```bash
python -m venv .venv
python -m pip install --upgrade pip
python -m pip install watcherobot
```

激活虚拟环境的命令因系统而异：

```powershell
# Windows PowerShell
.\.venv\Scripts\Activate.ps1
watcherobot --version
```

```sh
# macOS / Linux
source .venv/bin/activate
watcherobot --version
```

## 配对机器人

交互式设置会完成网络配置、配对并检查最终连接：

```bash
watcherobot robot setup
```

设备已在正确网络中时，可以在机器人上打开 **Python SDK** Application，然后使用屏幕上的六位配对码：

```bash
watcherobot robot pair 123456
watcherobot robot status
```

## 创建第一个 Application

```bash
watcherobot app init hello_robot
cd hello_robot
watcherobot app run
```

始终使用 `watcherobot app run` 启动 Application，不要直接运行 `python app.py`。只有通过 Daemon 启动，Application 才能获得正确的设备与桌面通道。

## 源码与完整参考

SDK 的 CLI、媒体能力、Application Marketplace 和示例仍以 [WatcheRobot Python SDK 仓库](https://github.com/orulink-ai/WatcheRobot_python_sdk)中的版本化文档为准。本网站会逐步收敛稳定的公开使用路径，不复制内部实现记录。
