# 真实 LVGL 主机视图回归

独立 CMake/CTest 工程，直接编译 `main/check_view.c`、`main/check_layout.c` 和真实 LVGL 的非 ESP 库。只 mock ESP 内存、显示锁、FreeRTOS 输入队列、已解码图像的释放；图像缩放链接现有 `common/demo_state.c`。无假 LVGL、SDL、硬件连接或 framebuffer 驱动。

## 最终回归结果

2026-10-08，Windows x64 / MSVC 19.40，真实 LVGL **8.4.0**，guard 与显式 image attempt 接口落地后：

| 配置 | 结果 | CTest 用时 |
| --- | --- | --- |
| Release | 21/21 通过，0 失败 | 0.47s |
| Debug | 21/21 通过，0 失败 | 0.91s |

`release_outside` 与 `release_disabled` 均由真实运行红转绿；弧外直接 release、setting_pending/cancelling/stopping 下直接 release 都不入队。第 3 个 Screen 触点移至 y198 后，running 内容不再与 status 重叠。显式 `check_view_image(image, attempt)` 的事件先于 model render 到达用例通过，图像不会绑定到过期 shown attempt。

新增测试与 mock 在 MSVC `/W4 /WX` 下成功编译，Debug/Release 均保持断言有效。只改本 host 目录，没有修改主代码、CI、common，没有提交或操作硬件。

软件截图目录：`build/snapshots/`（相对此主机工程目录）。21 个场景各有一个 PPM，包括 `nav_pixels.ppm`、`detail_tail.ppm`、`stopping.ppm`、`photo.ppm`、`live_image.ppm`、`release_outside.ppm`、`release_disabled.ppm`。Debug/Release 共用输出目录，最后执行的配置会覆盖同名截图和 `build/Testing/Temporary/LastTest.log`。全部是软件渲染数据，不是 LCD 实机证明。

## 运行

在本目录运行（PowerShell 与 macOS/Linux Shell 使用相同 CMake 命令，无额外 shell 脚本）：

```text
cmake -S . -B build -DLVGL_SOURCE_DIR=<真实的managed_components/lvgl__lvgl绝对路径> -DHOST_WRITE_PPM=ON
cmake --build build --config Release --target check_view_tests --parallel
ctest --test-dir build -C Release --output-on-failure
```

单配置生成器还应在 configure 时传 `-DCMAKE_BUILD_TYPE=Release`；Windows 可显式传 `-G "Visual Studio 17 2022" -A x64`。

固件 `main/idf_component.yml` / `dependencies.lock` 和本轮本地库均为 **LVGL 8.4.0**。根据最新合同，工程严格要求真实 8.4.0；配置时读取版本头，运行时也断言版本，提供其他版本会明确失败，不修改/伪造库版本。

CI 从 ESP32 子仓库根目录运行以下三个命令；真实 LVGL 8.4.0 checkout 在 `.ci/lvgl`。`$PWD` 写法在 PowerShell 与 macOS/Linux Shell 均可使用：

```text
cmake -S examples/hardware_sdk/hardware_check/test_support/host -B examples/hardware_sdk/hardware_check/test_support/host/build-ci -DLVGL_SOURCE_DIR="$PWD/.ci/lvgl" -DCMAKE_BUILD_TYPE=Release -DHOST_WRITE_PPM=ON
cmake --build examples/hardware_sdk/hardware_check/test_support/host/build-ci --config Release --target check_view_tests --parallel
ctest --test-dir examples/hardware_sdk/hardware_check/test_support/host/build-ci -C Release --output-on-failure
```

失败退出码由 build/CTest 原样返回；可归档 `build-ci/snapshots/*.ppm` 与 `build-ci/Testing/Temporary/LastTest.log`。不依赖 ESP-IDF，不运行 firmware build/flash，也不修改 CI。

`lv_conf.h` 开启 RGB565、snapshot 和 Montserrat 14/16/18/20/24/48。所有测试源通过 `#ifdef NDEBUG / #error` 防止断言失效；MSVC 用 `/UNDEBUG`，其他编译器用 `-UNDEBUG`，Release 同样执行断言。新增测试和 mock 目标使用 `/W4 /WX`（其他编译器 `-Wall -Wextra -Werror`）；生产代码与 LVGL 是独立依赖目标，保留警告而不要求上游库零警告。注意字体名的 px 大小不等于其 `line_height`（18px 的行高实际为 21）。

## 验证合同

| 场景 | 实际检查 |
| --- | --- |
| config | 真库版本、RGB565、6 个字体的实际 glyph、412×412 snapshot |
| geometry_oracle | 独立椭圆/圆屏解析 oracle 与生产 hit/span API 全像素一致，越界和中央空白无命中 |
| nav_geometry | 两个 178×124 区域在 (0,288)/(234,288)，真实 `lv_obj_hit_test` 和 `LV_EVENT_HIT_TEST` 全像素一致；输入驱动实际点击 |
| nav_pixels | 隐藏导航字形后对 y288..411 所有像素核对左灰、右浅绿与背景；圆屏外/弧外/中央空白无绘制 |
| labels | 左右中心 (108,340)/(304,340)，常态 18px，主动作深字；Not OK/Next 同中心 |
| content/status | 标题 y80/中心 x206，常态 visual 中心 (206,188)，实际控制器状态 y260、bottom≤280，互不重叠 |
| running_content | 各步骤运行时所有可见内容不与 status 重叠，包括 Screen 第三个触点 |
| report/detail_tail | 两页报告和所有详情页的真实行/字形边界，41px 行高；短列表在 y114..277 的 164px 区间居中；Lights 末页两行单独回归，无标题/导航重叠 |
| stop | STOP 20px、红色/按下深红、真实点击产生右意图；取消等待仍红且 disabled，返回 idle 恢复常态 |
| stopping | STOP_TEST 保持 RUNNING，新增 stopping 只增加右侧 disable 条件；仍为红 STOP，输入 model.status 为 Stop requested; waiting，显示文案为 Stopping \| waiting for result，无重复点击入队；ACK 后恢复 review 的 Next |
| pressed/native_input | 普通按下仍浅绿、释放恢复；弧内滑到弧外/空白/圆屏外无误触，按住后变 disabled 无入队，解除禁用后各产生一次意图 |
| release_outside | 按下右区后，下一个 pointer 样本直接在弧外/中央空白/圆屏外 release；计数断言恰好两个样本，不能插入 pressing 移动样本 |
| release_disabled | 按住后原地 render 成 disabled，然后直接 release；render 期间不调用 timer，不允许中间 pressing 样本掩盖缺陷；覆盖 setting_pending、cancelling、stopping |
| busy | 所有步骤 running/cancelling、setting_pending、welcome 的禁用状态与真实输入，禁用浅灰，音量 slider 禁用 |
| photo/live_image | 移交所有权，thumbnail 224×126，放大 320×180；真实变换和 snapshot 图像像素边界中心 (206,188)，不碰 eyebrow/状态/导航；放大隐藏大标题，保留指定 eyebrow，返回与 Next 意图合同不变 |
| stale_image | 无图、跨 attempt、换步骤不得假放大；本轮图替换释放旧数据，Together 仍允许同轮 retained 图 |
| image_arrival_attempt | shown 仍为旧 attempt，直接调用固定新 API `check_view_image(image, new_attempt)`，随后 render 新 attempt 必须显示新照片；迟到旧 attempt 不能显示 |

生产 geometry 的独立 oracle 使用像素中心：圆心 (205.5,205.5)、半径 206；左椭圆 rx112/ry100，右侧镜像 `411-x`，左区域面积 11043 像素。oracle 不调用生产 predicate 来推导期望值。

图像源使用已解码的 320×180 RGB565 洋红色数据。thumbnail 由真实整数 zoom 和软件采样得到，允许 1px 采样量化边缘；放大 1:1 为 (46,98)..(365,277)。原始 LVGL img 对象仍可能是源图尺寸，测试同时检查真实 transformed area 和 snapshot 可见像素，避免把源图对象尺寸误当作缩略图尺寸。

## 软件截图

`HOST_WRITE_PPM=ON` 时测试程序直接写 `build/snapshots/<scenario>.ppm`，失败也会保存当时画面。也可运行 `build/Release/check_view_tests photo --ppm <文件路径>`（单配置生成器的程序在 build 根目录）。仅为 LVGL 软件渲染证据，**不是 LCD 实机证明**。构建产物和 PPM 均在本目录忽略，不提交。

## 首轮红结果

2026-10-08，Windows x64 / MSVC 19.40，Release，实际 LVGL 8.4.0，旧版 `check_view.c`：13 项，3 通过（config、geometry_oracle、report），10 项预期失败。

- 导航仍为 100×52；逐像素形状/颜色有 23832 个不一致。
- Back 中心实际 (148,330)，合同 (108,340)。
- 标题/状态尚未迁移到 y80/y260；STOP 仍非 20px，禁用色尚未调整。
- 照片中心仍 (206,206)，合同 (206,188)。
- `full_image=true` 但无本轮 retained image 时，旧视图错误隐藏 status。

最近一次原始输出保存在 `build/Testing/Temporary/LastTest.log`（每次 CTest 会覆盖；首轮红结果以上述记录为准）。本工程不修改主代码、CI 或 common。

## 第一轮移植后的新增红点

扩充到 18 项后，14 通过、4 失败：

- `running_content`：Screen running 第三个触点 (180,221)..(231,272) 与 status (66,260)..(345,277) 重叠。
- `report` / `detail_tail`：短列表仍从 y114 开始；两行末页应从 y155 开始，一行列表应从 y175 开始。
- `stopping`：当时模型字段尚未加入。交付版已删除临时探测/兼容逻辑，直接编译固定 `stopping` 字段和 `main/check_layout.c`；API 缺失即编译失败，不能跳过场景。

首轮移植的照片变换检查曾因测试把 LVGL 每侧 2px 的采样余量误当成 1px 而失败，已校正；可见图像像素边界仍严格限制在目标矩形的 1px 量化范围内。真实 thumbnail/放大、停止取消、按下色、禁用和导航滑出均已通过。

上一轮固定 API 版本：Release 构建通过，18 项中 **17 通过、1 失败**。短列表居中和 stopping 文案/禁用/红 STOP/保持 RUNNING 均已转绿。剩余失败是 `check_view_running_content` 的第三触点与 status 重叠；现有 52px 高度要求其顶部 y≤208，才能让底边低于 status 的 y260。

## 直接 release 的 TDD 新增回归

新增后共 21 个场景。Release 构建成功后仅运行三项新增用例，观察到：

- `release_outside` 红：右区 (304,340) 按下，下一样本直接在弧外 (237,290) release，实际错误入队 1 条意图。
- `release_disabled` 红：按住后 setting_pending render 成 disabled，下一样本直接 release，实际错误入队 1 条意图。旧 `render()` helper 自带 timer，会插入 pressing 样本而掩盖这个缺陷，因此新用例直接调用生产 render 并断言 pointer read 计数未变化。
- `image_arrival_attempt` 绿：主代理在此次测试完成构建前已提供新的显式 attempt 实现，没有观察到旧实现的该项运行红结果，不伪造基线。

图像相关全部用例已直接使用固定的 `(demo_image_t *, uint32_t attempt)` API，无旧签名探测或兼容。guard 修复后已分别 build/CTest Debug 和 Release 全量，各 21 项通过，见本文最终回归结果。MSVC 同一 build 目录可通过 `--config Debug` / `--config Release` 选择配置；CTest 对应使用 `-C Debug` / `-C Release`。
