"""Contracts for public metadata, reproducible test dependencies and real locks."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]
SDK = ROOT / 'firmware/sdk'


def test_component_metadata_points_at_public_delivery():
    inventory = {row['destination']: row for row in json.loads((SDK / 'SOURCE.json').read_text(encoding='utf-8'))['files']}
    for name in ('hal/hal_audio', 'hal/hal_camera', 'protocols/mcu_link'):
        rel = f'components/{name}/idf_component.yml'
        text = (SDK / rel).read_text(encoding='utf-8')
        assert 'url: "https://github.com/orulink-ai/WatcheRobot-public"' in text
        assert inventory[rel]['transform'] == 'public repository metadata URL'


def test_sscma_host_dependency_has_reproducible_public_template():
    relative = 'components/sscma_client/test_support/host/CMakeLists.txt'
    generated = (SDK / relative).read_bytes()
    assert generated == (ROOT / 'tools/templates/sscma-client-host.cmake').read_bytes()
    text = generated.decode('utf-8')
    assert 'CJSON_SOURCE_DIR' in text
    assert '/UNDEBUG' in text and '-UNDEBUG' in text
    workflow = (ROOT / '.github/workflows/firmware-sdk.yml').read_text(encoding='utf-8')
    assert 'c859b25da02955fef659d658b8f324b5cde87be3' in workflow
    assert '-DCJSON_SOURCE_DIR=' in workflow
    assert '-notmatch "sscma_client"' not in workflow


def test_real_public_dependency_locks_cover_each_example():
    for example in ('body', 'head', 'robot', 'hardware_check'):
        lock = SDK / 'examples' / example / 'dependencies.lock'
        text = lock.read_text(encoding='utf-8')
        assert 'version: 6.0.2' in text
        assert 'target: esp32s3' in text
        assert 'firmware/s3' not in text
        assert '/home/' not in text
        if example != 'body':
            assert 'version: 8.4.0' in text
