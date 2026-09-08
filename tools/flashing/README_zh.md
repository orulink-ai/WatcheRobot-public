<p><a href="README.md">English</a> | <strong>简体中文</strong></p>

# AI 辅助固件烧录

权威烧录 Skill 位于当前 Release 的 `PTL-paired` ZIP 内，路径为 `skills/watche-ptl-release-flash/SKILL.md`，不需要单独安装。

解压配套 ZIP，用 AI 编程助手打开该目录，然后输入：

```text
请读取 skills/watche-ptl-release-flash/SKILL.md，准备所需环境，
校验配套包，识别 CH342 的两个接口，并按 Skill 执行。
```

在执行 Himax 与 ESP32-S3 配套流程前，先使用 ST-LINK/SWD 烧录 STM32F103。入口见[固件烧录指引](../../docs/flashing.md)。
