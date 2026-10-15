"""Optional source contracts; no SDK imports, network calls, or hardware access."""

import ast
import os
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
SDK = os.environ.get("WATCHER_DOCS_SDK_SOURCE")
ESP32 = os.environ.get("WATCHER_DOCS_ESP32_SOURCE")


class DocumentedExamples(unittest.TestCase):
    def test_python_examples_parse(self):
        for path in (ROOT / "wiki/guides").rglob("capabilities.md"):
            for code in re.findall(r"```python\n(.*?)```", path.read_text(encoding="utf-8"), re.S):
                ast.parse(code, filename=str(path))

    @unittest.skipUnless(SDK, "set WATCHER_DOCS_SDK_SOURCE to the SDK source checkout")
    def test_documented_methods_exist(self):
        source = Path(SDK) / "src/watcherobot"
        expected = {
            "robot.py": {
                "MotionDomain": ["move_to", "set_target", "play_action", "stop"],
                "AudioDomain": ["play", "play_file", "play_pcm", "stop"],
                "MicrophoneDomain": ["open", "open_pcm", "record_pcm"],
                "CameraDomain": ["capture", "capture_with_feedback"],
                "LightsDomain": ["set_color", "play_effect", "off"],
                "ExpressionRuntimeDomain": ["start", "update", "stop"],
                "AnimationDomain": ["available_ids", "play"],
                "BehaviorDomain": ["play"],
                "WorkDomain": ["play", "play_expression"],
                "WatcheRobot": ["supports", "capabilities"],
            },
            "vision.py": {
                "VisionDomain": ["models", "start_inference", "stop_inference"],
                "FaceTrackingDomain": ["start", "stop", "open_preview"],
            },
            "recordings.py": {"RecordingsDomain": ["start", "start_host", "heartbeat"]},
            "application/context.py": {"ApplicationContext": ["from_environment"]},
            "application/rtc.py": {"ApplicationRtc": ["start", "stop", "reset"]},
        }
        for file, classes in expected.items():
            tree = ast.parse((source / file).read_text(encoding="utf-8"))
            actual = {node.name: {item.name for item in node.body
                                  if isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef))}
                      for node in tree.body if isinstance(node, ast.ClassDef)}
            for name, methods in classes.items():
                with self.subTest(file=file, cls=name):
                    self.assertLessEqual(set(methods), actual.get(name, set()))
        cli = (source / "cli.py").read_text(encoding="utf-8")
        for option in ['"configure"', '"--service"', '"--template"', '"run"']:
            self.assertTrue(option in cli, f"Missing CLI option: {option}")
        self.assertTrue("add_distribution_commands(app_commands)" in cli)
        distribution = (source / "distribution/cli.py").read_text(encoding="utf-8")
        tree = ast.parse(distribution)
        commands = {node.args[0].value for node in ast.walk(tree)
                    if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                    and node.func.attr == "add_parser" and node.args
                    and isinstance(node.args[0], ast.Constant)}
        self.assertIn("check", commands)
        self.assertTrue((source / "templates/voice/config/models/llm.toml").is_file())

    @unittest.skipUnless(ESP32, "set WATCHER_DOCS_ESP32_SOURCE to the ESP32 source checkout")
    def test_capability_identifiers_and_provisioning_contract(self):
        source = Path(ESP32) / "firmware/s3"
        protocol = (source / "components/sdk/watcher_sdk/src/watcher_sdk_protocol.c").read_text(encoding="utf-8")
        guide = (ROOT / "wiki/guides/products/watcherobot/capabilities.md").read_text(encoding="utf-8")
        identifiers = re.findall(r"`([a-z]+(?:\.[a-z_]+)+\.v\d+)`", guide)
        self.assertGreater(len(identifiers), 10)
        for identifier in identifiers:
            self.assertIn('"' + identifier + '"', protocol, identifier)
        onboarding = (source / "main/provisioning_onboarding.c").read_text(encoding="utf-8")
        self.assertIn("watcher://provision?v=1&device_id=%s", onboarding)
        self.assertIn("Disconnect clears saved Wi-Fi", onboarding)
        main = (source / "main/app_main.c").read_text(encoding="utf-8")
        self.assertIn('"Desktop Link"', main)
        self.assertIn("BUTTON_SHUTDOWN_HOLD_MS 3000", main)


if __name__ == "__main__":
    unittest.main()
