English | [简体中文](action-test_zh.md)

# First-run Check

Power on only after flashing and SD-card preparation, then run this minimum check:

1. The display reaches the normal interface without staying black or rebooting repeatedly.
2. Follow the [device guide](../wiki/guides/en/products/watcherobot/device-guide.md): Phone Control for phones; configure Wi-Fi and open Desktop Link for a computer/SDK.
3. Confirm that the selected client is connected, not merely discovering the device or saving pairing.
4. Test an expression and a sound separately; not every expression includes audio.
5. Test a light effect and a small movement separately. Movement does not verify lighting, and BLE connection does not verify actual STM32 motion.
6. When using the SDK, run `watcherobot robot status` and confirm that device status is returned.

If a step fails, record that step and the screen or script message, then return to the matching section of the [Flashing Guide](flashing.md).
