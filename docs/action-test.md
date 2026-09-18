English | [简体中文](action-test_zh.md)

# First-run Check

Power on only after flashing and SD-card preparation, then run this minimum check:

1. The display reaches the normal interface without staying black or rebooting repeatedly.
2. Open Phone Control, wait a few seconds, and connect the phone over BLE.
3. Desktop or the App can discover and connect to the robot.
4. Play one expression and confirm the display, sound, and SD-card resources.
5. Run one small movement and confirm the STM32, servos, and lights.
6. When using the SDK, run `watcherobot robot status` and confirm that device status is returned.

If a step fails, record that step and the screen or script message, then return to the matching section of the [Flashing Guide](flashing.md).
