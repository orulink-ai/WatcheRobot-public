<p><a href="flashing.md">English</a> | <strong>简体中文</strong></p>

# 从组装到烧录

按顺序完成：**组装 → 烧身体主板 → 烧 Watcher 头部 → 准备 SD 卡 → 上电使用**。

## 1. 下载并准备工具

打开 [Releases](https://github.com/orulink-ai/WatcheRobot-public/releases)，从标记为 Latest 的版本下载三个附件：

- 名称含 `STM32` 的 ZIP：用于身体内部的主板。
- 名称含 `PTL-paired` 的 ZIP：用于上方的 Watcher 头部，已经包含 Himax 和 ESP32 固件以及烧录脚本。
- 名称含 `sd-resources` 的压缩包：用于 SD 卡。

按页面标注的版本下载即可，不需要手动校验。组装材料和步骤见[立创开源硬件项目](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl)。

电脑需要安装 Python 3.10+。Windows 安装时勾选添加 Python 到 PATH，安装后重新打开终端。另需 ST-LINK、OpenOCD、CH342 串口驱动、USB 数据线和 SD 读卡器。只烧录发布包，不需要安装固件编译环境。

## 2. 烧录身体内部的 STM32 主板

1. 完成组装后断电检查接线，再把 ST-LINK 接到身体主板标注的 SWD 接口。
2. 按板卡供电要求给主板供电，把 ST-LINK 接到电脑。
3. 解压 STM32 ZIP，进入能看到 `watcheRobot_STM32.bin` 的文件夹。在这里打开终端：Windows 可在文件夹地址栏输入 `powershell` 后回车。
4. 运行下面的命令：

```text
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program watcheRobot_STM32.bin verify reset exit 0x08000000"
```

出现 `Verified OK` 且命令成功结束后，断电再移除 ST-LINK。如果提示找不到 `openocd`，先将 OpenOCD 的 bin 目录加入 PATH，再重新打开终端。

## 3. 为 Watcher 头部创建 Python 环境

解压 `PTL-paired` ZIP，进入**同时包含 `requirements.txt` 和 `tools` 文件夹**的那一层，在这里打开终端。接下来的命令都在这个文件夹运行。

Windows PowerShell 逐行运行：

```powershell
python --version
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
```

macOS/Linux 逐行运行：

```sh
python3 --version
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

第一条查看 Python 是否安装；第二条在当前文件夹创建专用环境；第三条安装脚本依赖。等待安装成功后继续。后面直接使用这个环境里的 Python，**不用激活环境**。

## 4. 找到头部的两个串口

用 USB 数据线连接 Watcher 头部，在刚才的终端运行：

Windows：

```powershell
.\.venv\Scripts\python.exe -m serial.tools.list_ports -v
```

macOS/Linux：

```sh
.venv/bin/python -m serial.tools.list_ports -v
```

找到同一 CH342 设备的两个端口：**SERIAL-B / MI_02 是 ESP32 控制口，SERIAL-A / MI_00 是 Himax 口**。Windows 可结合设备管理器的端口属性确认。不要按 COM 数字大小猜；未出现两个端口时，先检查 USB 数据线和 CH342 驱动。

## 5. 运行头部烧录脚本

下面 Windows 命令假设控制口是 COM5、Himax 口是 COM6。**将这两个端口改成上一步实际查到的端口再运行。**

```powershell
.\.venv\Scripts\python.exe tools\ptl_release.py flash --port COM5 --vision-port COM6
```

macOS/Linux 同样替换两处端口路径：

```sh
.venv/bin/python tools/ptl_release.py flash --port /dev/CONTROL_PORT --vision-port /dev/VISION_PORT
```

脚本会自动检查包内文件，再依次烧录 Himax 和 ESP32。等待出现 `PTL paired flash completed.`，期间不要拔线。出现失败提示时停止，保留错误内容排查。

### 可选：让 AI 按 Skill 操作

Skill 是给 AI 助手读取的操作说明，不是 Python 脚本。用 AI 编程助手打开解压后的配套包文件夹，发送：

> 请读取 skills/watche-ptl-release-flash/SKILL.md，创建烧录环境，识别已连接 Watcher 的两个串口，然后执行配套烧录并检查结果。

手动执行上面的步骤时不需要另外安装 Skill。

## 6. 准备 SD 卡，再上电使用

1. 断电取出 Watcher 头部的 SD 卡，用读卡器连接电脑。SD 卡只能通过读卡器准备。
2. 备份卡上需要保留的文件，将卡格式化为 FAT32。
3. 将 `sd-resources` 压缩包内容解压到卡根目录，不要只复制压缩包，也不要额外套一层文件夹。
4. 确认卡根目录能看到 `assets/`、`official_catalog.json` 和 `resource_manifest.json`。
5. 安全弹出 SD 卡，断电插回头部，然后上电。

最后安装 Release 中适合自己系统的客户端，按[首次启动检查](action-test_zh.md)确认运行情况。使用 Python 控制机器人时，继续看 [SDK 安装与运行](sdk_zh.md)。
