# Public SDK sync validation / 公开同步验证事实

## Scope / 范围

- Fixed upstream / 固定来源: `orulink-ai/WatcheRobot_esp32` commit `ab4e324e2b15a1be5431c34de106f05d55ccb99e`.
- Delivery / 交付: public branch `feat/embedded-sdk-watcher-hw-public-sync`; use the draft PR's current head for public checkout, not the upstream SHA.
- `SOURCE.json`: 429 allowlisted source files with upstream/output SHA256 and explicit transforms. The three public-only `esp_lvgl_port/test_support/host` files are preserved from public HEAD; public guides, exporter, tests and CI are maintained outside this upstream inventory.
- Excluded / 不导出: official app/business services, models, credentials, `.a` archives, `test_apps`, audio `test_support/privacy`, generated builds and managed dependencies.
- No merge, production deployment, hardware flash or Release publication is authorized by this record. / 本记录不代表合并、生产部署、烧设备或发布 Release。

## Gates / 门禁

| Gate / 检查 | This public snapshot / 这份公开快照 |
| --- | --- |
| Export reproducibility / 导出幂等 | PASS: exact repeated export / 重复导出逐字节一致 |
| Python, Node, static Wiki / 自动回归 | PASS with explicit skips / 通过，跳过项见下 |
| Host CTest and real LVGL 8.4.0 / 主机测试 | Selected suites PASS; SSCMA blocked / 选定套件通过，SSCMA 阻塞 |
| Public ESP-IDF 6.0.2, all four examples / 公开 IDF 四工程 | **Pending**: parent agent will build the public tree and generate new dependency locks; old locks were not restored. / 父 agent 后续构建并生成新锁，不恢复旧锁冒充验证。 |
| On-screen full-travel safety warning regression / 屏幕全行程警示 | **Pending**: separate upstream patch is being prepared; parent agent must apply it to this public snapshot, update provenance and rerun affected tests/builds. Documentation warnings do not fix missing device UI warnings. / 独立上游补丁待父 agent 应用并更新来源、重测重构建；文档警示不替代屏幕修复。 |
| Robot acceptance / 实机验收 | **NOT PERFORMED in this task**: no flashing, paired STM32/Himax qualification, full motion/STOP, touch, optical/acoustic or long-duration acceptance. / 本任务未烧录、未配对与实机验收。 |
| Official recovery / 官方恢复 | **Pending** physical verification; first custom flash overwrites official `agentcfg` 0x12000–0x20000; backup and complete official restore are mandatory. / 首刷覆盖配置，必须备份及完整官方恢复，实机验证未完成。 |

`VALIDATION.md`, `REVIEW.md` and `history/UPSTREAM_VALIDATION.md` retain historical claims, names and hashes. Their prior builds are **not** proof that this public snapshot builds or is safe on hardware. / 历史记录不篡改，不把原 source 构建冒充当前公开构建。

The [development preview Release](https://github.com/orulink-ai/WatcheRobot-public/releases/tag/hardware-check-preview-20261030) is **pending publication**. Downloaded binary provenance follows that Release, independently of this source inventory. / 开发预览待发布，包来源跟随 Release，不默认等于本清单。

## Actual commands and results / 真实命令与结果

- Repeated `python tools/export_firmware_sdk.py --source <pinned checkout> --ref ab4e324e2b15a1be5431c34de106f05d55ccb99e`: initially all 443 SDK file path/hash pairs unchanged (428 exported + 15 public-owned files at that time). The parent concurrently added the required shared PTL frame header plus its regression and exporter mapping; a final re-export normalized its manifest ordering, then repeat verification used the final 429-file upstream inventory. A hermetic fixture additionally verifies stale-file removal, exclusions and preservation of the public LCD harness. / 真实完整快照哈希一致，独立夹具覆盖清理与保留。
- `python -m pytest tools/tests -q`: **64 passed, 6 skipped** on Python 3.11.15. Five PowerShell tests skip because `pwsh` is unavailable; SD formatter Windows probe skips on Linux. Initial bare `pytest` failed imports; module invocation fixes repository import discovery. The existing flash-environment success test was made interpreter-independent by mocking supported Python 3.12; production Python 3.12 policy is unchanged. / 正确使用模块入口，修复成功测试的宿主版本依赖。
- `npm test`: **17 passed**. `npm run docs:build`: **PASS**, 66 Wiki pages built. / 站点测试与构建通过。
- CMake configure/build + `ctest --output-on-failure --no-tests=error`, **Debug and Release**, 18 selected suites each: **35 non-LVGL CTests + 21 actual LVGL CTests per configuration**, all pass. Real library is LVGL v8.4.0 commit `4495f428630cc1741bd8bfd977f080e8460e8e8d`, not a UI shim. Restored public LCD staging tests initially failed against the upstream mutex/helper split; the harness now extracts both real production functions and mocks the mutex API; **2/2 pass in both configurations**. No production LCD implementation was changed for that repair.
- Additional SSCMA harness configure was attempted in both configurations and **BLOCKED** by missing `managed_components/espressif__cjson/cJSON/cJSON.c`. It is explicitly excluded from the self-contained host CI set, not counted as passing; resolve its dependency separately before claiming that additional suite. / SSCMA 缺 cJSON，不能算通过。
- Windows host and PowerShell execution are configured in CI but **not locally executed**. Public ESP-IDF and lock generation are deliberately deferred to the parent agent. The `python-sdk` submodule is uninitialized; its independent suite was not run (no Python SDK code changed). / 不冒充跨平台、IDF 或独立 Python SDK 测试。
- Working-tree `git diff --check` initially passed, but staged/base comparison also includes newly tracked upstream `media_lib_sal` files and reports inherited trailing whitespace/new blank EOF lines there. These vendor files are intentionally byte-preserved for source hashes, not silently reformatted. No original `VALIDATION.md` or `REVIEW.md` edits. / 初次工作区检查未包含未跟踪文件；暂存完整比较发现第三方原始空白告警，保持来源字节不静默改写；历史验证未改。

All pending safety, public-IDF and hardware gates above remain blocking regardless of host success. / host 通过不能解除上述屏幕安全、公开 IDF 和实机 pending 门禁。
