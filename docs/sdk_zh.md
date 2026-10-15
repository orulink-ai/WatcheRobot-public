[English](sdk.md) | 简体中文

# Python SDK 快速开始

在电脑上编写 Application，通过 SDK 控制机器人，不需要重新烧录自定义 ESP32 固件。本文按 SDK **0.1.12** 核对，支持 Python **3.10–3.12**，推荐 3.11。公开仓库的 `python-sdk/` 可能固定在较早版本，不代表当前安装包版本。

## 1. 安装 SDK

安装 [Miniconda](https://docs.conda.io/projects/miniconda/en/latest/)，在支持 Conda 的终端执行（Windows 可用 Anaconda PowerShell Prompt）：

```sh
conda create -n watcherobot-sdk python=3.11 pip -y
conda activate watcherobot-sdk
python -m pip install --upgrade pip
python -m pip install watcherobot
watcherobot --version
```

使用 [Release](https://github.com/orulink-ai/WatcheRobot-public/releases) 的 `.whl` 时，在同一环境执行 `python -m pip install "下载的完整whl文件路径"`，不要同时安装多份不同来源的 SDK。先确认安装包支持所需接口；升级安装包不等于桌面端正在使用的 Daemon 已升级。

截至 2026-10-15，本次核查的 SDK 最新 GitHub 正式包为 **0.1.11**，源码版本号 **0.1.12 尚无对应正式包**。需要复现本页核对的源码版本（包括新版语音凭据向导）时，先安装 [Git](https://git-scm.com/downloads)，再在专用环境中执行以下命令替代正式包安装；这是固定源码安装，不是正式发行版：

```sh
python -m pip install "watcherobot @ git+https://github.com/orulink-ai/WatcheRobot_python_sdk.git@cc0a31993b4fe1a607e75ad920ad013e5ddc934a"
watcherobot --version
```

## 2. 联网并配对

先按[设备使用指南](../wiki/guides/products/watcherobot/device-guide.md#wifi)完成手机扫码配网。电脑与机器人连接同一局域网，在机器人上打开 **Desktop Link**，读取当前六位配对码：

```sh
watcherobot robot pair 123456
watcherobot robot status
```

将 `123456` 换成实际配对码，以状态显示已连接为准。旧固件可能仍将入口命名为 **Python SDK**。

没有使用手机时，可在具备蓝牙的 Windows/macOS 电脑运行 `watcherobot robot setup`，按终端提示完成配网和配对；手机扫码路径不要求电脑有蓝牙。

## 3. 创建并运行 Application

```sh
watcherobot app init hello_robot
cd hello_robot
watcherobot app check .
watcherobot app run
```

基础模板只建立应用生命周期，不会自动让机器人运动。打开生成的 `app.py`，在 `ApplicationContext.from_environment()` 上下文中使用 `app.robot`；桌面消息使用 `app.desktop`。同步设备调用放到 `asyncio.to_thread(...)` 中，避免阻塞异步通道。

**使用 `watcherobot app run` 启动，不要直接执行 `python app.py`。** Daemon 负责设备连接和应用生命周期，Application 负责业务。`app check` 通过只代表工程检查通过，不代表设备或云模型已验证。

## 4. 继续开发

- [机器人原子能力](../wiki/guides/products/watcherobot/capabilities.md)：摄像头、音频、动作、灯光、表情和输入。
- [快速创建语音对话](../wiki/guides/products/watcherobot/voice.md)：内置 voice 模板连接 ASR、LLM 和 TTS。
- [SDK 与 Application 的关系](../wiki/guides/sdk/index.md)。
- [SDK 0.1.12 源码与完整参考](https://github.com/orulink-ai/WatcheRobot_python_sdk/tree/cc0a31993b4fe1a607e75ad920ad013e5ddc934a)。

遇到问题，先记录 `watcherobot --version`、`watcherobot daemon status` 和 `watcherobot robot status`。不要反复启动不同版本的 Daemon；停止或切换运行时会影响正在运行的应用。
