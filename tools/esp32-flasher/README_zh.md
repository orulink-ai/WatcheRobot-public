<p><a href="README.md">English</a> | <strong>简体中文</strong></p>

# 旧版 ESP32 烧录工具

> 本工具只适用于带 `flash_args.txt` 的旧 ESP32 单芯片 ZIP，不能用于当前 `PTL-paired` 配套包，也不能烧录 Himax。

当前 Watcher 头部烧录请使用配套 ZIP 内的 Skill 和工具，入口见[固件烧录指引](../../docs/flashing_zh.md)。

旧包命令：

```powershell
python -m pip install -r tools/win_flasher/requirements.txt
python -m tools.win_flasher list-ports
python -m tools.win_flasher flash --zip .\LEGACY-ESP32-PACKAGE.zip --port COM7 --monitor
```
