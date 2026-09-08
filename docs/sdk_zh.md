[English](sdk.md) | 简体中文

# SDK 安装与运行

先完成机器人烧录、SD 卡准备和联网。从 [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest) 下载 `watcherobot` 的 `.whl` 文件，在该文件所在目录打开终端。

## 1. 创建环境并安装

```bash
conda create -n watcherobot-sdk python=3.12 pip -y
conda activate watcherobot-sdk
```

Windows PowerShell：

```powershell
$wheel = Get-Item .\watcherobot-*.whl
python -m pip install $wheel.FullName
watcherobot --version
```

macOS/Linux：

```bash
python -m pip install ./watcherobot-*.whl
watcherobot --version
```

## 2. 配置并连接机器人

首次配置网络时，在支持蓝牙的 Windows 或 macOS 电脑运行：

```bash
watcherobot robot setup
```

机器人联网后，让电脑连接同一网络，并在机器人上打开 Python SDK 应用。将 `123456` 换成屏幕显示的六位配对码：

```bash
watcherobot robot pair 123456
watcherobot robot status
```

## 3. 运行 Python 脚本

```bash
python my_robot.py
```

将 `my_robot.py` 换成自己的脚本文件。API 和示例见 [Python SDK 中文说明](../python-sdk/README.zh-CN.md)。
