# Development-preview acceptance / 开发预览验收

No stable release or fully validated firmware pairing is claimed.
本页仅记录实际执行结果；构建成功不等于整机安全验证完成。

| Item / 项目 | Status / 状态 |
| --- | --- |
| Public body/head/robot builds | Windows IDF 6.0.2 passed / Windows 构建通过 |
| Separate developer project | Windows IDF 6.0.2 passed; hardware pending / 新工程构建通过，实机待验 |
| Windows/Linux remote CI | Pending / 待运行 |
| STM32 commit/version | Not recorded / 未记录 |
| Himax PTL SHA256 | Not recorded / 未记录 |
| Hardware: motion, lights, touch, JPEG, PCM, display | Pending / 待验收 |

2026-10-06: ten Windows MSVC host projects passed in Debug and Release
(22 CTest executions, including UART concurrency tests). Public tool and SDK
contract pytest suite passed (62 tests, including Windows/Shell script parity). Builds
resolved dependencies in new project directories using the installed IDF and
local download cache; this is not a clean-machine installation claim.

Release gates: clean-checkout setup and all builds; ordered events and observable
overflow; motion limits/stop/rejection/completion/fault; no reconnect replay; JPEG
decode and stream restart; PCM 16k/24k and volume; screen touch; concurrent lifecycle;
missing/incompatible hardware isolation; custom flashing and official restoration.
Record board revision, SDK commit, STM32 version, Himax checksum and evidence.
