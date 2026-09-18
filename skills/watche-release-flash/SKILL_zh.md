[English](SKILL.md) | 简体中文

# 发布固件烧录

Windows 使用仓库 `tools/flash.ps1`，macOS/Linux 使用 `tools/flash.sh`。按指南用 Conda 新建并进入命名环境（示例 `watcherobot`）；同名环境已存在时换一个新名字，未经用户确认不复用已有环境。其余依赖和烧录工具由脚本准备。入口拒绝 base 和解释器不匹配的环境，禁止向 base、用户包目录或其他已有 Python 环境安装依赖，不修改全局 PATH 和 Conda 配置。缺少 Conda 时说明前置条件，不回退到系统 Python。

先读[烧录指南](../../docs/flashing_zh.md)，确定已解压固件目录和用户授权的目标。STM32 参数为 `stm32 --package <包含bin的目录>`；头部参数为 `head --package <配套包目录> --port <控制口> --vision-port <视觉口>`。路径以仓库目录为基准，含空格时加引号。

头部 SERIAL-B / MI_02 为 ESP32 控制口，填写 `--port`；SERIAL-A / MI_00 为 Himax 口，填写 `--vision-port`。先运行不带端口的头部命令，使用脚本打印的三列表确认实际参数；需要复查时使用指南中的只读端口查询命令。确认两者 USB serial number 相同。入口会检查 A/B 角色，填反时必须停止并按提示修正。工具先写 Himax 再写 ESP32。不改固件、不换版本。

公开 Release 的首次完整安装必须在头部命令末尾添加 `--factory`，让 ESP32 分区表、应用和 storage 来自同一发布包。该参数会覆盖原有 ESP32 数据；已有设备要求保留数据时停止并说明，不把更新请求擅自改成完整安装。

`--prepare-only` 仅下载和检查工具，不访问硬件，不算实机验收。Windows 驱动安装可能触发管理员权限要求，保留系统提示，失败即停止。驱动下载失败不能报告准备完成。CH342 自动驱动安装及 Linux 权限配置目前尚未实现，遇到时如实报告。

STM32 要求退出码为零，并包含 Programming Finished、Verified OK、Resetting Target。头部要求依次出现 Himax 完成、PTL paired 完成和仓库入口完成提示。模拟和环境准备不能代替实际烧录成功。SD 卡仅通过读卡器和仓库写卡脚本安装资源。

SD 卡插回并上电后再做整机验收：正常进入主界面；Phone Control 能打开；手机 BLE 能连接；一个 SD 表情资源可播放；一个小幅动作能完成。首次打开 Phone Control 可等待几秒完成 BLE 和 STM32 链路初始化。若某项失败，只报告已确认的阶段，不把“烧录成功”等同于“整机功能通过”。
