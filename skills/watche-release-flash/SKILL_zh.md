[English](SKILL.md) | 简体中文

# 发布固件烧录

Windows 使用仓库 `tools/flash.ps1`，macOS/Linux 使用 `tools/flash.sh`。按指南用 Conda 新建并进入命名环境（示例 `watcherobot`）；同名环境已存在时换一个新名字，未经用户确认不复用已有环境。其余依赖和烧录工具由脚本准备。入口拒绝 base 和解释器不匹配的环境，禁止向 base、用户包目录或其他已有 Python 环境安装依赖，不修改全局 PATH 和 Conda 配置。缺少 Conda 时说明前置条件，不回退到系统 Python。

先读[烧录指南](../../docs/flashing_zh.md)，确定已解压固件目录和用户授权的目标。STM32 参数为 `stm32 --package <包含bin的目录>`；头部参数为 `head --package <配套包目录> --port <控制口> --vision-port <视觉口>`。路径以仓库目录为基准，含空格时加引号。

头部 SERIAL-B / MI_02 为控制口，SERIAL-A / MI_00 为视觉口，确认两者 USB serial number 相同。工具先写 Himax 再写 ESP32。不改固件、不换版本、不以工厂擦除绕过故障。

`--prepare-only` 仅下载和检查工具，不访问硬件，不算实机验收。Windows 驱动安装可能触发管理员权限要求，保留系统提示，失败即停止。驱动下载失败不能报告准备完成。CH342 自动驱动安装及 Linux 权限配置目前尚未实现，遇到时如实报告。

STM32 要求退出码为零，并包含 Programming Finished、Verified OK、Resetting Target。头部要求配套命令成功结束，另查启动。模拟和环境准备不能代替实际烧录成功。SD 卡仅通过读卡器复制资源。
