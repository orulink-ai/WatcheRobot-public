English | [简体中文](action-test_zh.md)

# First-run Check

Power on only after flashing and SD-card preparation, then run this minimum check:

1. The display reaches the normal interface without staying black or rebooting repeatedly.
2. Desktop or the App can discover and connect to the robot.
3. Play one expression and confirm the display and sound.
4. Run one small movement and confirm the servos and lights.
5. When using the SDK, run `watcherobot robot status` and confirm that device status is returned.

If a step fails, record that step and the screen or script message, then return to the matching section of the [Flashing Guide](flashing.md).
