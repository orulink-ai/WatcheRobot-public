"""Check that the public SDK is self-contained and excludes official services."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SDK = ROOT / "firmware/sdk"

def test_public_sdk_contains_all_entry_points():
    for module in ("core", "body", "camera", "audio", "display"):
        assert (SDK / f"components/sdk/watche_hw_{module}/include/watche_hw_{module}.h").is_file()
    for example in ("body", "head", "robot"):
        assert (SDK / f"examples/{example}/main/app_main.c").is_file()
    assert (SDK / "LICENSE").is_file()

def test_sdk_does_not_reference_private_checkout():
    cmake = (SDK / "examples/sdk.cmake").read_text(encoding="utf-8")
    assert "${CMAKE_CURRENT_LIST_DIR}/../components" in cmake
    assert "firmware/s3" not in cmake
    camera = (SDK / "components/hal/hal_camera/CMakeLists.txt").read_text(encoding="utf-8")
    assert "hx6538_video_bridge" not in camera

def test_only_hardware_components_are_published():
    for name in ("watcher_sdk", "voice_service", "behavior_state_service", "ws_client", "hal_display"):
        assert not list((SDK / "components").rglob(name))
    for extension in ("*.bin", "*.elf", "*.exe"):
        assert not [p for p in SDK.rglob(extension)
                    if not any(part.startswith("build") or part == "managed_components"
                               for part in p.relative_to(SDK).parts)]
