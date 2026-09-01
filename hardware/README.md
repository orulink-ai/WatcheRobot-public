# Hardware Package

This directory contains the public hardware reproduction materials for WatcheRobot.

## Package Map

| Board or asset | Schematic | Layout | Gerber | BOM | CPL | Source |
| --- | --- | --- | --- | --- | --- | --- |
| Feedback servo STM32 co-processor board | `pcb/schematic/SCH_反馈舵机STM32协控制板_2026-06-11.pdf` | `pcb/layout/PCB_反馈舵机STM32协控制板_2026-06-11.pdf` | `pcb/gerber/Gerber_反馈舵机STM32协控制板_2026-06-11/` | `pcb/bom/BOM_反馈舵机STM32协控制板_2026-06-18.xlsx` | `pcb/cpl/CPL_反馈舵机STM32协控制板_2026-06-17.xlsx` | `pcb/pcb-source/ProPrj_WatcheRobot2.0openSource_2026-06-17.epro` |
| Foot charging board | `pcb/schematic/SCH_脚底充电板原理图_2026-06-11.pdf` | `pcb/layout/PCB_脚底充电板PCB_2026-06-11.pdf` | `pcb/gerber/Gerber_脚底充电板PCB_2026-06-11/` | `pcb/bom/BOM_脚底充电板PCB_2026-06-18.xlsx` | `pcb/cpl/CPL_脚底充电板PCB_2026-06-17.xlsx` | `pcb/pcb-source/ProPrj_WatcheRobot2.0openSource_2026-06-17.epro` |
| Wireless charging base | `pcb/schematic/SCH_无线充电底座原理图_2026-06-15.pdf` | `pcb/layout/PCB_无线充电底座PCB_2026-06-15.pdf` | `pcb/gerber/Gerber_无线充电底座PCB_2026-06-15/` | `pcb/bom/BOM_无线充电底座PCB_2026-06-18.xlsx` | `pcb/cpl/CPL_无线充电底座PCB_2026-06-17.xlsx` | `pcb/pcb-source/ProPrj_WatcheRobot2.0openSource_2026-06-17.epro` |
| Side LED board | `pcb/schematic/SCH_侧边灯板原理图_2026-06-15.pdf` | `pcb/layout/PCB_侧边灯板_2026-06-15.pdf` | `pcb/gerber/Gerber_侧边灯板_2026-06-15/` | `pcb/bom/BOM_侧边灯板_2026-06-18.xlsx` | `pcb/cpl/CPL_侧边灯板_2026-06-17.xlsx` | `pcb/pcb-source/ProPrj_WatcheRobot2.0openSource_2026-06-17.epro` |
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
