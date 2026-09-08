<p><a href="README.md">English</a> | <strong>简体中文</strong></p>

# 硬件资料

物料和装配步骤见[立创开源硬件项目](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl)。本目录保存 PCB、BOM、贴片坐标、Gerber 和机械模型。

| 实物 | 资料位置 |
| --- | --- |
| 身体主板，也就是反馈舵机 STM32 控制板 | `pcb/` 中名称包含 `STM32Servo` 的文件 |
| 脚底充电板 | `pcb/` 中名称包含 `FootCharger` 的文件 |
| 无线充电底座 | `pcb/` 中名称包含 `WirelessBase` 的文件 |
| 侧边灯板 | `pcb/` 中名称包含 `SideLight` 的文件 |
| 机械结构 | `3d-models/exports/WatcherRobot-mian.stp` |

先完成装配并断电检查接线，再开始烧录。身体主板使用 ST-LINK/SWD 烧录 STM32；上方 Watcher 头部通过 USB 烧录 Himax 和 ESP32-S3。
