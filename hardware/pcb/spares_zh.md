[English](spares.md) | 简体中文

# 物料与装配

先看整机组装料，再按需要下载各板卡 BOM。下列链接直接指向仓库现有 Excel，不再维护一张空白备用物料表。

| 资料 | 用途 |
| --- | --- |
| [整机组装料](bom/BOM_Assembly_整机组装料.xlsx) | 整机采购、组装物料参考；约 68 MiB，建议下载后查看 |
| [STM32 反馈舵机控制板 BOM](bom/BOM_STM32Servo_反馈舵机控制板.xlsx) | 身体主板元件 |
| [脚底充电板 BOM](bom/BOM_FootCharger_脚底充电板.xlsx) | 脚底充电板元件 |
| [无线充电底座 BOM](bom/BOM_WirelessBase_无线充电底座.xlsx) | 充电底座元件 |
| [侧边灯板 BOM](bom/BOM_SideLight_侧边灯板.xlsx) | 侧边灯板元件 |

装配说明见[立创开源硬件项目](https://oshwhub.com/team_efhmhuqf/project_gbxcghnl)，打印模型见 [MakerWorld](https://makerworld.com.cn/zh/models/2911544-watcherobot)。PCB 工程、Gerber、贴片坐标与机械模型位置见[硬件资料](../README_zh.md)。

按实际板卡版本核对 BOM、Gerber 和 CPL，不混用不同版本。不要将本文的文件列表当作库存或已采购数量；元件型号、数量、螺丝和线材规格以对应清单及装配说明为准，缺项应向维护者确认，不能猜测替代。

完成装配、断电核对接线后，继续[烧录与首次使用](../../docs/flashing_zh.md)。
