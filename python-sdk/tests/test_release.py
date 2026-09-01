from pathlib import Path


ROOT = Path(__file__).parents[1]
REPO_ROOT = ROOT.parent


def test_package_version_has_one_alpha_source() -> None:
    pyproject = (ROOT / "pyproject.toml").read_text(encoding="utf-8")
    package_init = (ROOT / "src" / "watcherobot" / "__init__.py").read_text(encoding="utf-8")

    assert 'dynamic = ["version"]' in pyproject
    assert '[tool.hatch.version]\npath = "src/watcherobot/__init__.py"' in pyproject
    assert 'version = "0.1.0"' not in pyproject
    assert '__version__ = "0.1.0a4"' in package_init


def test_main_ci_runs_sdk_tests() -> None:
    workflow = (REPO_ROOT / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")

    assert 'python-version: ["3.10", "3.11", "3.12"]' in workflow
    assert "working-directory: python-sdk" in workflow
    assert 'python -m pip install -e ".[test]"' in workflow
    assert "python -m pytest" in workflow


def test_main_ci_enforces_public_boundaries() -> None:
    workflow = (REPO_ROOT / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")

    assert "firmware/esp32-s3" in workflow
    assert "firmware/stm32-f103" in workflow
    assert "Closed-source implementation directories must not be committed." in workflow
    assert "Release artifacts must be uploaded to GitHub Releases, not committed to Git." in workflow
    assert "Potential secret detected." in workflow
