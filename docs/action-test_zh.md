[English](action-test.md) | 简体中文

# 首次运行检查

完成烧录和 SD 卡准备后再上电，做一次最小检查：

1. 屏幕进入正常界面，没有停留在黑屏或反复重启。
2. 按[设备使用指南](../wiki/guides/products/watcherobot/device-guide.md)选择入口：手机使用 Phone Control，电脑/SDK 先完成配网再打开 Desktop Link。
3. 确认所选客户端已连接；只发现设备或保存配对关系不算完成。
4. 分别播放表情和音效，确认屏幕与扬声器；不是每个表情都自带声音。
5. 分别测试灯效和小幅动作，观察灯光与舵机。动作成功不能代替灯效检查，BLE 连接也不能代替 STM32 实际动作检查。
6. 使用 SDK 时，运行 `watcherobot robot status`，确认能返回设备状态。

任一步失败时，先记录失败步骤和屏幕或脚本提示，再回到[烧录指南](flashing_zh.md)对应章节检查。
