[English](../en/guide/assembly.md) | 简体中文

# 整机拼装教程

本页配合整机 BOM、部件动画和完整视频，演示如何把 WatcheRobot 的结构件、电子模块、线材和螺丝组装成整机。开始前先完成 [PCB 制作与 SMT 贴片](../../../docs/pcb-fabrication_zh.md)，并下载[整机组装 BOM](../../../hardware/pcb/bom/BOM_Assembly_整机组装料.xlsx)。具体数量、线材和紧固件规格以 BOM 为准，不要只凭视频画面采购。

::: warning 安全要求
拼装、插拔排线和调整接线时保持断电。不要强扭舵机，不要仅按线色判断接口；电池极性、排线方向和螺丝长度不确定时先暂停核对。
:::

## 1. 认识主要部件 {#parts}

### 结构件与主要模块

![WatcheRobot 结构件与主要模块展示](/media/docs/assembly/01_parts_reveal_preview_full_cover.gif)

| 部件 | 用途与检查 |
| --- | --- |
| Watcher 头部 | 提供屏幕、摄像头、麦克风和扬声器；确认接口与身体结构匹配。 |
| 身体、后盖、中轴、底座和底盖 | 组成机身与运动结构；安装前去除毛刺并试装孔位。 |
| 轴承 | 支撑中轴转动；确认装入方向和平顺度。 |
| 防滑垫 | 安装在底部，待整机检查完成后粘贴。 |
| STM32 身体控制板 | 连接舵机、头部和身体电子部件。 |
| 侧边灯板 | 安装在身体侧边，用于灯光显示。 |

### 电子模块和连接线

![WatcheRobot 电子模块和连接线展示](/media/docs/assembly/02_electronics_reveal_full_cover.gif)

包括电池、灯光部件、调试线、FPC 排线、底部供电板、Type-C 连接线、触摸铜箔、两只反馈舵机和供电连接线。小型加工件的名称与安装位置以 BOM 和视频实物为准。

### 螺丝和紧固件

![WatcheRobot 螺丝和紧固件展示](/media/docs/assembly/03_screws_reveal_full_cover.gif)

素材展示了 M2×6、M2×8 和 M2×20 三组螺丝。不同长度不能混用；具体数量和安装位置以 BOM、壳体孔深和视频步骤为准，锁紧前确认螺丝不会顶住 PCB、电池或活动机构。

## 2. 完整拼装视频 {#video}

<video controls playsinline preload="metadata" poster="/media/docs/assembly/assembly-poster.jpg" style="width: 100%; border-radius: 12px;">
  <source src="https://wiki.orulink.ai/media-assets/assembly/watcherobot-assembly-tutorial-720p-7cd0f58e.mp4" type="video/mp4">
  当前浏览器无法播放视频，请使用下方章节时间轴定位内容。
</video>

视频约 9 分 54 秒。预览版已压缩为 720p，画面内容与原视频保持一致。

| 时间 | 阶段 |
| --- | --- |
| 00:00 | 清点部件，准备身体机构与舵机 |
| 00:20 | 安装身体内部舵机与连接件 |
| 01:20 | 固定底部结构和相关小板 |
| 02:00 | 安装底部供电板和外部接口 |
| 03:20 | 连接 Watcher 头部与身体控制板 |
| 04:30 | 整理头部、身体和控制板线束 |
| 05:00 | 安装身体外壳、触摸铜箔与 FPC 排线 |
| 05:30 | 对齐壳体孔位并固定长螺丝 |
| 06:20 | 合拢身体结构并放置控制板 |
| 07:00 | 接入控制板、舵机和其余线束 |
| 08:20 | 安装电池并整理内部空间 |
| 09:00 | 安装后盖、底盖并完成外观检查 |

时间点用于快速定位，实际动作以视频连续画面和 BOM 为准。
