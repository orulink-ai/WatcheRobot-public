# Public SDK sync validation / 公开 SDK 同步验证

## Fixed delivery / 固定交付

- Public repository: `orulink-ai/WatcheRobot-public`, PR #14. The PR head or the associated test-firmware Release tag identifies the public delivery; do not check out the private upstream SHA in this public repository.
- Upstream source: `orulink-ai/WatcheRobot_esp32` commit `e23bee2a642f6c90753b370cb18c46815e0c8f58`, including the on-screen reference-motion warning from internal PR #290.
- `SOURCE.json` inventories 429 allowlisted upstream files, their original/output hashes and every public export adaptation. Public-only LCD tests and generated public dependency locks are preserved separately.
- Excluded: official application/business services, models, credentials, generated builds, managed component trees, binary archives, codec test apps and unrelated audio privacy harnesses.

## Actual validation / 本次真实验证

| Gate | Result |
| --- | --- |
| Fixed-source export | **PASS:** repeated export of all 448 SDK files has identical paths and SHA256, including the public-owned test harnesses and dependency locks. |
| Python tool/contract suite | **69 passed, 6 skipped** on Linux/Python 3.11.15. Five PowerShell cases require unavailable `pwsh`; the SD formatter Windows probe is not run on Linux. |
| Wiki navigation/link tests | **17 passed**. The static build is verified separately for the exact preview commit. |
| Public host C tests | **19 suites per configuration, 58 CTests each in Debug and Release, all passed.** This includes the actual LVGL view and SSCMA with explicitly supplied cJSON. |
| Real UI safety regression | **22/22 in Debug and 22/22 in Release** against real LVGL 8.4.0, commit `4495f428630cc1741bd8bfd977f080e8460e8e8d`. Includes warning visibility, text size, circular aperture geometry, confirmation input and leaving-page cleanup. |
| Public-path ESP-IDF builds | **All four passed:** body, head, robot and hardware_check with ESP-IDF 6.0.2 / ESP32-S3. Built from an isolated tree containing only public SDK component/example paths; no private checkout or official top-level application. |
| Generated dependencies | Each example's `dependencies.lock` is taken from the actual public-path build. Old locks were not reused. |
| Firmware provenance | All **293 compiler/configuration input files** collected from the final build were checked by SHA256 against the public working tree before packaging. Release manifest records the immutable public source commit, upstream commit, build inputs and artifact hashes. |
| Firmware image validation | Actual bootloader/application ESP32-S3 images pass esptool 5.3.0 checksum and validation-hash checks; the real partition table is parsed. |
| Device flashing | **NOT PERFORMED.** No robot was flashed or operated in this task. |
| Current paired hardware / official recovery | **NOT ACCEPTED by this work.** No current STM32/Himax paired qualification, physical screen/touch, acoustic/image quality, mechanical limits/stop or long-duration acceptance is claimed. |

The [independent hardware-check preview Release](https://github.com/orulink-ai/WatcheRobot-public/releases/tag/hardware-check-preview-20261030) is a prerelease, not the official stable product upgrade. Availability and exact binary provenance follow that Release and its manifest. Publishing the test firmware does not merge this documentation PR or deploy the production Wiki.

## Reproducible local commands / 本地复现

From the public repository root:

```sh
python -m pytest tools/tests -q
npm ci
npm test
npm run docs:build
```

For all host projects, configure Debug and Release separately, build, then run `ctest --output-on-failure --no-tests=error`. Set `LVGL_SOURCE_DIR` to LVGL commit `4495f428630cc1741bd8bfd977f080e8460e8e8d` and `CJSON_SOURCE_DIR` to cJSON 1.7.19 commit `c859b25da02955fef659d658b8f324b5cde87be3`. CI fetches these exact public commits and includes SSCMA; missing cJSON gives an explicit setup error rather than a hidden private dependency. Assertions remain active in Release.

For firmware, activate ESP-IDF 6.0.2 and run `idf.py -C firmware/sdk/examples/<EXAMPLE> build` for body, head, robot and hardware_check. These are build commands only, not flashing instructions. Use the bilingual getting-started guide before any device write.

## Corrections found during verification / 本轮发现并修复

- The public PTL camera build initially lacked `watcher_face_frame.h`. The shared protocol header is now exported with provenance; no private Himax implementation or include-tree access is required.
- The preserved public LCD test harness predated the new panel mutex/helper split. It now exercises both actual production functions, mutex ownership/contention, timeout storage retention and partial final bands without changing production LCD code to satisfy a test.
- SSCMA originally assumed a private managed cJSON path. It now accepts an explicit public dependency directory, uses a pinned public CI checkout and retains Release assertions.
- Three component metadata URLs pointed to a retired organization. The exporter now converts only their metadata URL and records the transform.
- Windows CI exposed implicit text-encoding and CMake path assumptions in the snapshot tests. They now specify UTF-8 and portable CMake paths; Linux regressions include a simulated legacy default encoding. Actual Windows results remain the GitHub checks for the exact head, not a local Windows claim.
- Generated header/dependency changes require a fresh configure. The final build includes the source/UI warning correction and refreshed public metadata, rather than relabeling the earlier private-tree build.

## Hardware and historical boundaries / 硬件与历史边界

Current servos have **no physical position feedback**. ACK/DONE/STOPPED and command coordinates do not prove measured position, accurate arrival, obstruction detection or physical emergency stopping. Reference X90/Y120 can cause large travel from an unknown start. Clear the whole travel and maintain a reliable manual power-disconnection option.

First custom flashing starts the application at 0x10000 and overwrites the original official `agentcfg` area 0x12000–0x20000. Back up required data and prepare a matching full official restore package; flashing the old application alone is not a complete restore.

`VALIDATION.md`, `REVIEW.md` and `history/UPSTREAM_VALIDATION.md` retain their original historical names, hashes and claims. Those earlier checks are not relabeled as acceptance of this delivery. Third-party `media_lib_sal` source whitespace is byte-preserved rather than silently reformatted to remove inherited diff warnings.

Windows host/PowerShell execution is checked by CI, not locally on this Linux host. The independent Python SDK submodule suite was not run locally because no Python SDK implementation was changed. No production deployment or merge is implied by this record.
