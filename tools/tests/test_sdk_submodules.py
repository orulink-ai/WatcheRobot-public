"""Public repository SDK boundaries; SDK implementation tests live upstream."""

import configparser
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def test_both_sdks_are_pinned_public_submodules():
    config = configparser.ConfigParser()
    config.read(ROOT / ".gitmodules", encoding="utf-8")
    for path, repository in (
        ("python-sdk", "WatcheRobot_python_sdk"),
        ("c-sdk", "WatcheRobot_c_sdk"),
    ):
        section = config[f'submodule "{path}"']
        assert section["path"] == path
        assert section["url"] == f"https://github.com/orulink-ai/{repository}.git"
        entry = subprocess.check_output(
            ["git", "ls-files", "--stage", "--", path], cwd=ROOT, text=True
        ).strip().split()
        assert entry[0] == "160000", entry
        assert len(entry[1]) == 40
        assert entry[3] == path


def test_no_duplicate_firmware_or_private_snapshot_export_pipeline():
    assert not (ROOT / "firmware").exists()
    assert not (ROOT / "tools/export_firmware_sdk.py").exists()
    assert not (ROOT / ".github/workflows/firmware-sdk.yml").exists()
    for suffix in ("", "_zh"):
        guide = (ROOT / f"docs/embedded-sdk{suffix}.md").read_text(encoding="utf-8")
        assert "WatcheRobot_c_sdk" in guide
        assert "git submodule" not in guide
        assert "firmware/" not in guide
        assert "本 public 仓库" not in guide
        assert "Git 历史" not in guide
        assert "Old Wiki" not in guide
