"""Public delivery regression: namespace, no-feedback hardware and reproducible export."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SDK = ROOT / 'firmware/sdk'


def test_all_current_hardware_names_use_watcher():
    for path in (SDK / 'components/sdk').rglob('*'):
        if path.is_file():
            assert 'watche_hw_' not in str(path)
            assert 'watche_hw_' not in path.read_text()
    for name in ('body', 'head', 'robot', 'hardware_check'):
        assert f'project(watcher_hw_{name})' in (SDK / f'examples/{name}/CMakeLists.txt').read_text()


def test_snapshot_inventory_and_public_boundary():
    manifest = json.loads((SDK / 'SOURCE.json').read_text())
    assert re.fullmatch('[a-f0-9]{40}', manifest['source_commit'])
    for item in manifest['files']:
        path = SDK / item['destination']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256'], path
    for forbidden in ('watcher_sdk', 'voice_service', 'behavior_state_service', 'ws_client'):
        assert not list((SDK / 'components').rglob(forbidden))
    for extension in ('*.pem', '*.key', '*.bin', '*.elf', '*.a', '.env'):
        assert not list(SDK.rglob(extension))
    assert not list(SDK.rglob('test_apps'))
    assert not list(SDK.rglob('privacy'))
    host = SDK / 'components/esp_lvgl_port/test_support/host'
    for name in ('CMakeLists.txt', 'test_lcd_dma_release.c', 'test_lcd_dma_staging.c'):
        assert (host / name).is_file()


def test_current_guides_have_safe_public_commands_and_no_feedback():
    for lang in ('', '_zh'):
        guide = (SDK / f'docs/getting-started{lang}.md').read_text()
        for required in ('6.0.2', 'git checkout', 'hardware_check', 'watcher_hw_',
                         'app-flash', 'Ctrl+]', 'STM32', 'Himax', 'SOURCE.json'):
            assert required in guide
        check = (SDK / f'docs/hardware-check{lang}.md').read_text()
        for required in ('26', 'X90', 'Y120', 'NOT_SUPPORTED', 'STOP', 'diag status'):
            assert required in check
        assert 'fresh feedback' not in check
        assert '500 ms window' not in check
    for name in ('body', 'robot'):
        assert 'request_position(' not in (SDK / f'examples/{name}/main/app_main.c').read_text()


def test_public_delivery_contracts():
    workflow = (ROOT / '.github/workflows/firmware-sdk.yml').read_text()
    for required in ('hardware_check', 'watcher_hw_core', 'examples/common',
                     'LVGL_SOURCE_DIR', '4495f428630cc1741bd8bfd977f080e8460e8e8d'):
        assert required in workflow
    assert 'watche_hw_' not in workflow
    for lang in ('', '_zh'):
        guide = (SDK / f'docs/getting-started{lang}.md').read_text()
        for required in ('Python 3.10+', 'SERIAL-B', '0x12000', '0x20000',
                         'hardware-check-preview-20261030', '0x0'):
            assert required in guide
        servo = (SDK / f'docs/servo-open-loop{lang}.md').read_text()
        assert 'ACK/DONE' in servo and 'X90/Y120' in servo
    assert (SDK / 'SYNC_VALIDATION.md').is_file()


def test_export_is_idempotent_and_keeps_public_host_harness(tmp_path):
    import importlib.util
    spec = importlib.util.spec_from_file_location('sdk_export', ROOT / 'tools/export_firmware_sdk.py')
    assert spec is not None and spec.loader is not None
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)
    source = tmp_path / 'source'
    source.mkdir()
    subprocess.run(['git', 'init', str(source)], check=True, capture_output=True)
    def put(name, content):
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    put('LICENSE', 'test license')
    put('firmware/s3/components/sdk/watcher_hw_core/core.c', '/* core */')
    put('firmware/s3/components/hal/hal_audio/test_support/privacy/private.c', 'private')
    put('firmware/s3/components/esp_codec_dev/test_apps/main.c', 'test app')
    put('firmware/s3/components/esp_codec_dev/private.a', 'archive')
    subprocess.run(['git', '-C', str(source), 'add', '.'], check=True)
    subprocess.run(['git', '-C', str(source), '-c', 'user.name=Test', '-c',
                    'user.email=test@example.invalid', 'commit', '-m', 'fixture'], check=True, capture_output=True)
    ref = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD']).decode().strip()
    public_root = tmp_path / 'public'
    setattr(exporter, 'ROOT', public_root)
    sdk = public_root / 'firmware/sdk'
    host = sdk / 'components/esp_lvgl_port/test_support/host/CMakeLists.txt'
    host.parent.mkdir(parents=True)
    host.write_text('public regression')
    def snapshot():
        return {str(p.relative_to(sdk)): p.read_bytes() for p in sdk.rglob('*') if p.is_file()}
    exporter.export(source, ref)
    first = snapshot()
    stale = sdk / 'components/stale.c'
    stale.write_text('stale')
    exporter.export(source, ref)
    assert snapshot() == first
    assert host.read_text() == 'public regression'
    assert not any('privacy' in p or 'test_apps' in p or p.endswith('.a') for p in first)


def test_public_ptl_camera_includes_shared_frame_header():
    """The public head build must not borrow the private Himax include tree."""
    relative = 'components/hal/hal_camera/include/watcher_face_frame.h'
    assert (SDK / relative).is_file()
    manifest = json.loads((SDK / 'SOURCE.json').read_text())
    assert any(item['destination'] == relative for item in manifest['files'])
    camera = (SDK / 'components/hal/hal_camera/CMakeLists.txt').read_text()
    assert 'hx6538_video_bridge' not in camera


def test_selected_components_resolve_without_private_checkout(tmp_path):
    fake_idf = tmp_path / 'idf/tools/cmake'
    fake_idf.mkdir(parents=True)
    (fake_idf / 'project.cmake').write_text('')
    import os
    for head in ('ON', 'OFF'):
        probe = tmp_path / 'probe.cmake'
        selected = tmp_path / 'selected.txt'
        probe.write_text(f'set(HW_HEAD {head})\nset(HW_BODY ON)\n'
            f'include("{SDK / "examples/sdk.cmake"}")\n'
            f'file(WRITE "{selected}" "${{EXTRA_COMPONENT_DIRS}}")\n')
        subprocess.run(['cmake', '-P', str(probe)], check=True,
                       env=dict(os.environ, IDF_PATH=str(fake_idf.parents[1])), capture_output=True)
        dirs = [Path(p) for p in selected.read_text().split(';')]
        assert all((p / 'CMakeLists.txt').is_file() for p in dirs)
        assert any(p.name == 'media_lib_sal' for p in dirs) == (head == 'ON')
