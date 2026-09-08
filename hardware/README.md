<p><strong>English</strong> | <a href="README_zh.md">简体中文</a></p>

# Hardware Package

This directory contains the public hardware reproduction materials for WatcheRobot.

For the public hardware project and material entry, see the [WatcheRobot OSHW project](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl). Complete assembly and power-off wiring checks before firmware flashing.

## Package Map

| Board or asset | Schematic | Layout | Gerber | BOM | CPL | Source |
| --- | --- | --- | --- | --- | --- | --- |
| Feedback servo STM32 co-processor board | `pcb/schematic/SCH_STM32Servo_反馈舵机控制板.pdf` | `pcb/layout/PCB_STM32Servo_反馈舵机控制板.pdf` | `pcb/gerber/Gerber_STM32Servo_反馈舵机控制板/` | `pcb/bom/BOM_STM32Servo_反馈舵机控制板.xlsx` | `pcb/cpl/CPL_STM32Servo_反馈舵机控制板.xlsx` | `pcb/pcb-source/ProPrj_WatcheRobot2.0openSource_2026-06-17.epro` |
| Foot charging board | `pcb/schematic/SCH_FootCharger_脚底充电板.pdf` | `pcb/layout/PCB_FootCharger_脚底充电板.pdf` | `pcb/gerber/Gerber_FootCharger_脚底充电板/` | `pcb/bom/BOM_FootCharger_脚底充电板.xlsx` | `pcb/cpl/CPL_FootCharger_脚底充电板.xlsx` | `pcb/pcb-source/ProPrj_WatcheRobot2.0openSource_2026-06-17.epro` |
| Wireless charging base | `pcb/schematic/SCH_WirelessBase_无线充电底座.pdf` | `pcb/layout/PCB_WirelessBase_无线充电底座.pdf` | `pcb/gerber/Gerber_WirelessBase_无线充电底座/` | `pcb/bom/BOM_WirelessBase_无线充电底座.xlsx` | `pcb/cpl/CPL_WirelessBase_无线充电底座.xlsx` | `pcb/pcb-source/ProPrj_WatcheRobot2.0openSource_2026-06-17.epro` |
| Side LED board | `pcb/schematic/SCH_SideLight_侧边灯板.pdf` | `pcb/layout/PCB_SideLight_侧边灯板.pdf` | `pcb/gerber/Gerber_SideLight_侧边灯板/` | `pcb/bom/BOM_SideLight_侧边灯板.xlsx` | `pcb/cpl/CPL_SideLight_侧边灯板.xlsx` | `pcb/pcb-source/ProPrj_WatcheRobot2.0openSource_2026-06-17.epro` |
| Mechanical assembly | - | - | - | - | - | `3d-models/exports/WatcherRobot-mian.stp` |

## BOM Notes

The current BOM workbooks include:

- designator
- quantity
- parameter or model
- component description
- footprint
- manufacturer or brand
- manufacturer part number
- LCSC number
- supplier
- supplier part number
- datasheet
- mounting method
- alternate part

Purchase URLs and spare quantities are not fully populated in the current BOM files. Use `pcb/spares.md` to track spare parts and links.

## Large File Note

`3d-models/exports/WatcherRobot-mian.stp` is the current public mechanical assembly model. Future larger mechanical exports should be published through GitHub Releases or Git LFS rather than normal Git history.
