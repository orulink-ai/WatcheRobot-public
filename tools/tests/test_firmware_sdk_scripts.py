"""Run developer entries without IDF or serial hardware."""
from pathlib import Path
import os
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "firmware/sdk/examples"


@pytest.mark.parametrize("platform", ["powershell", "bash"])
@pytest.mark.parametrize("arguments, message", [
    (["body", "flash"], "explicit Port"),
    (["head", "monitor"], "explicit Port"),
    (["invalid", "build"], "Example must"),
    (["Body", "build"], "Example must"),
    (["robot", "erase-flash"], "Action must"),
])
def test_invalid_input_is_error(platform, arguments, message):
    executable = shutil.which("pwsh" if platform == "powershell" else "bash")
    if platform == "bash" and os.name == "nt":
        git_bash = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Git/bin/bash.exe"
        executable = str(git_bash) if git_bash.exists() else None
    if executable is None:
        pytest.skip(f"{platform} not installed")
    if platform == "powershell":
        command = [executable, "-NoProfile", "-File", str(SCRIPTS / "hardware-sdk.ps1")]
    else:
        path = (SCRIPTS / "hardware-sdk.sh").as_posix()
        if os.name == "nt":
            path = "/" + path[0].lower() + path[2:]
        command = [executable, path]
    result = subprocess.run(command + arguments, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=20)
    assert result.returncode == 2, result.stderr
    assert message in result.stderr


def test_entries_only_delegate_to_selected_project():
    for suffix in ("ps1", "sh"):
        source = (SCRIPTS / f"hardware-sdk.{suffix}").read_text(encoding="utf-8")
        assert "idf.py" in source
        assert "flash_ptl" not in source
        assert "erase-flash" not in source


