English | [简体中文](README_zh.md)

# Hardware resources

See [parts and assembly](pcb/spares.md) for whole-robot and board BOMs, and the [OSHW project](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl) for assembly steps. This directory contains PCBs, BOMs, placement files, Gerbers, and mechanical models.

| Physical part | Files |
| --- | --- |
| Body board / STM32 feedback servo controller | Files containing `STM32Servo` under `pcb/` |
| Foot charging board | Files containing `FootCharger` under `pcb/` |
| Wireless charging base | Files containing `WirelessBase` under `pcb/` |
| Side light board | Files containing `SideLight` under `pcb/` |
| Mechanical assembly | `3d-models/exports/WatcherRobot-mian.stp` |

Complete assembly and unpowered wiring checks before flashing. Flash the body STM32 through ST-LINK/SWD; flash head Himax and ESP32-S3 through USB.
