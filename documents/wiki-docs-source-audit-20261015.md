# 文档源码核对记录：2026-10-15

本次仅修改公开仓库文档、文档导航/样式及对应测试，不同步 SDK 子模块、不修改固件、不烧录设备。旧文档工作区保留原状。

## 核对基线

| 来源 | revision | 用途 |
| --- | --- | --- |
| ESP32 本地源码 | `76995278515af30ea482dba92cd62233595018f4` | 扫码配网、Desktop Link、恢复出厂、能力公告 |
| Python SDK 本地源码 | `cc0a31993b4fe1a607e75ad920ad013e5ddc934a`，源码版本 0.1.12 | CLI、Application、能力 API、voice 模板 |
| 公开总仓 SDK 子模块 | `abb94222f2b31a4dd979e52cca5cdee54129e2df` | 保持引用，不冒充上述新 SDK |
| SDK 发布核查 | GitHub Latest 与 PyPI 为 0.1.11；PyPI 0.1.12 返回 404 | 正式包与固定源码安装分开写 |

这里的“核对”是文档与静态源码对应，不等于完整构建、固件配套发布、所有功能实机验收或付费模型验证。

## 依据

- ESP32 `firmware/s3/main/provisioning_onboarding.c`：无凭据开机引导、二维码 URI、网络页状态和 Disconnect 清除凭据提示。
- ESP32 `firmware/s3/main/app_main.c`：Desktop Link 联网门禁、3 秒关机阈值、扫码成功后的 BLE 释放与返回、NVS 和 app_center 重置、系统更新入口。
- ESP32 `firmware/s3/components/utils/device_identity/device_identity.c`：WR ID 从硬件 MAC 派生，不因 NVS 擦除重新随机分配。
- ESP32 `firmware/s3/components/sdk/watcher_sdk/src/watcher_sdk_protocol.c`：ready 能力声明；PTL 推理能力受构建条件影响。
- SDK `src/watcherobot/cli.py`、`application/context.py`、`templates/base/app.py`：托管 Application 启动和注入通道。
- SDK `robot.py`、`audio.py`、`vision.py`、`recordings.py`、`application/rtc.py`：公开方法、PCM 格式、资源占用与停止边界。
- SDK `templates/voice/`、`voice/configure.py`、`docs/voice-template.zh-CN.md`：生成模板、凭据向导、默认供应商及半双工行为。
- 本仓 `tools/flash_head.py`：先 ESP32，初始化 Himax，再写 Himax，最后请求重启。

## 图片与版本边界

设备指南图片和社区二维码来自已有文档工作区，不是本次真机截图。保留必要原图，以等高 contain 排版避免裁掉操作内容；图片随站点发布，不依赖飞书访问权限。
设备步骤已按上述源码核对；图片中的界面布局与号码仅作示例。未用微信实际验证群二维码有效期。旧 PDF 保留用于历史参考，但不再作为首页的当前使用说明入口。

## 验证方式

- `npm test`：生成链接、双语配对、图片路径、导航缺页兼容和安装步骤。
- `npm run docs:build`：完整生成与静态构建。
- `python -m unittest discover -s wiki/tests -p "test_*.py"`：更新器回归与文档源码合同。
- 设置 `WATCHER_DOCS_SDK_SOURCE`、`WATCHER_DOCS_ESP32_SOURCE` 指向实际本地源码后，源码合同测试会检查 API 方法、能力标识和配网合同；没有源码时明确跳过，不算通过。
- 本地浏览器检查中文、英文、手机布局、配网图片和语言跳转；不写设备、不调用模型服务。

## 本次结果

- `npm run check`：12 项 Node 测试通过，58 页文档生成和生产构建通过。
- 设置两个源码路径后运行 Python 测试：6 项通过，4 项部署符号链接测试按现有 Windows 条件跳过；3 项新增源码核对测试全部通过。
- 本地浏览器：1280px 桌面与 390px 手机视口检查，设备指南 15 张图片加载成功，无页面横向溢出，中英文切换到对应指南。
- `git diff --check` 通过。文档与已有 SDK 改动分别管理；本记录不表示已推送或发布。
