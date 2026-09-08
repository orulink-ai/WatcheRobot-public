<p><a href="sdk.md">English</a> | <strong>简体中文</strong></p>

# SDK 安装与运行

先完成机器人烧录和 SD 卡准备。以下步骤用于在电脑上通过 Python 控制机器人。

## 1. 准备下载目录

从 [Latest Release](https://github.com/orulink-ai/WatcheRobot-public/releases/latest) 下载 `watcherobot` 的 `.whl` 文件，单独放进一个文件夹，只保留本次下载的一份。电脑的 Python 需符合 Release 页面列出的支持范围。

进入放有 wheel 的文件夹并打开终端。Windows 可在文件夹地址栏输入 `powershell` 后回车。

## 2. 创建环境并安装

Windows PowerShell 逐行运行：

```powershell
python --version
python -m venv .venv
$wheel = Get-Item .\watcherobot-*.whl
.\.venv\Scripts\python.exe -m pip install $wheel.FullName
.\.venv\Scripts\watcherobot.exe --version
```

macOS/Linux 逐行运行：

```sh
python3 --version
python3 -m venv .venv
.venv/bin/python -m pip install ./watcherobot-*.whl
.venv/bin/watcherobot --version
```

安装结束后会输出 SDK 版本。无需激活环境；后续都使用这个文件夹内的程序。

## 3. 连接机器人

首次配置网络，在支持蓝牙的 Windows 或 macOS 电脑运行下面的 setup 命令，按提示完成：

```powershell
.\.venv\Scripts\watcherobot.exe robot setup
```

如果机器人已经联网，电脑连接到同一 Wi-Fi，在机器人上打开 Python SDK 应用，查看六位配对码。将下面的 `123456` 替换为实际配对码：

```powershell
.\.venv\Scripts\watcherobot.exe robot pair 123456
.\.venv\Scripts\watcherobot.exe robot status
```

macOS/Linux 将上述命令开头的 `..venvScriptswatcherobot.exe` 换成 `.venv/bin/watcherobot`。status 返回设备状态后，就可以开始使用 SDK。

## 4. 运行自己的 Python 脚本

将自己的脚本放在这个文件夹中。假设文件名为 `my_robot.py`，Windows 运行：

```powershell
.\.venv\Scripts\python.exe .\my_robot.py
```

macOS/Linux 运行：

```sh
.venv/bin/python ./my_robot.py
```

`my_robot.py` 是示例文件名，需要替换为你已保存的脚本。API 用法和示例见 [SDK 中文说明](../python-sdk/README.zh-CN.md)；示例额外依赖按其说明安装。
