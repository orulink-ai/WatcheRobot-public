"""Export the reviewed hardware-only allowlist from a pinned ESP32 Git tree.

No working-tree files, official application, models, binaries or credentials are read.
Usage: python tools/export_firmware_sdk.py --source /path/to/esp32 --ref FULL_SHA
Run only in a clean/reviewed public checkout; replaces allowlisted snapshot trees.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
COMPONENTS = (
    *(f'sdk/watcher_hw_{name}' for name in ('core', 'body', 'camera', 'audio', 'display')),
    *(f'services/{name}' for name in ('mcu_runtime', 'mcu_power_service', 'mcu_led_service',
      'mcu_sensor_service', 'motion_arbiter', 'mcu_motion_service', 'watcher_input_router')),
    'protocols/mcu_link', 'hal/hal_audio', 'hal/hal_camera', 'esp_lvgl_port',
    'esp_io_expander_pca95xx_16bit', 'hx_media_protocol', 'hx_ptl_protocol',
    'jpeg_integrity', 'sensecap-watcher', 'sscma_client', 'watcher_hx_ptl',
    'esp_codec_dev', 'media_lib_sal',
)
EXAMPLES = ('body', 'head', 'robot', 'hardware_check', 'common')
EXAMPLE_FILES = ('sdk.cmake', 'hardware-sdk.sh', 'hardware-sdk.ps1')
DOCS = ('MIGRATION.md', 'AUDIT.md', 'PRODUCT.md', 'DESIGN.md', 'UX_REVIEW.md')


def export(source, ref):
    if not re.fullmatch(r'[0-9a-f]{40}', ref):
        raise ValueError('ref must be a full immutable Git SHA')
    def git(*args):
        return subprocess.check_output(['git', '-C', str(source), *args])
    if git('rev-parse', ref + '^{commit}').decode().strip() != ref:
        raise ValueError('commit mismatch')
    files = git('ls-tree', '-r', '--name-only', ref).decode().splitlines()
    mappings = [(f'firmware/s3/components/{p}/', f'components/{p}/') for p in COMPONENTS]
    mappings += [(f'examples/hardware_sdk/{p}/', f'examples/{p}/') for p in EXAMPLES]
    exact = {f'examples/hardware_sdk/{p}': f'examples/{p}' for p in EXAMPLE_FILES}
    exact.update({f'examples/hardware_sdk/{p}': p for p in DOCS})
    exact['LICENSE'] = 'LICENSE'
    # PTL camera HAL consumes this shared frame codec. Export only the header,
    # not the Himax implementation or a dependency on a private directory.
    exact['firmware/hx6538_video_bridge/include/watcher_face_frame.h'] = 'components/hal/hal_camera/include/watcher_face_frame.h'

    exact['examples/hardware_sdk/VALIDATION.md'] = 'history/UPSTREAM_VALIDATION.md'
    selected = []
    for path in files:
        target = exact.get(path)
        for prefix, dest in mappings:
            if path.startswith(prefix):
                target = dest + path[len(prefix):]
                break
        if target is None:
            continue
        parts = Path(target).parts
        if '/test_support/privacy/' in path or '/test_apps/' in path or Path(path).suffix == '.a':
            continue
        if any(p.startswith('build') or p in ('managed_components', '__pycache__', '.git') for p in parts):
            continue
        # Hardware example diagnostics are source; generated images, secrets and firmware are not.
        if Path(path).suffix in ('.bin', '.elf', '.exe', '.pem', '.key') or '.env' in parts:
            raise ValueError(f'Unexpected sensitive/generated file in allowlist: {path}')
        selected.append((path, target))
    sdk = ROOT / 'firmware/sdk'
    # Public-only LCD harness adaptations are preserved; SSCMA has an explicit
    # reproducible template transform below.
    public_tests = sdk / 'components/esp_lvgl_port/test_support/host'
    preserved = {p.relative_to(sdk): p.read_bytes() for p in public_tests.rglob('*') if p.is_file()}
    # Dependency locks are generated from real public-path IDF builds, not
    # copied from upstream or silently replaced by a future snapshot export.
    for example in ('body', 'head', 'robot', 'hardware_check'):
        lock = sdk / 'examples' / example / 'dependencies.lock'
        if lock.is_file():
            preserved[lock.relative_to(sdk)] = lock.read_bytes()
    for path in (sdk / 'components', sdk / 'examples'):
        if path.exists():
            shutil.rmtree(path)
    inventory = []
    for path, target in selected:
        data = git('show', f'{ref}:{path}')
        original_hash = hashlib.sha256(data).hexdigest()
        transform = None
        if target == 'examples/sdk.cmake':
            data = data.replace(b'../../firmware/s3/components', b'../components')
            transform = 'public-relative component root'
        elif target == 'components/hal/hal_camera/CMakeLists.txt':
            data = data.replace(b' "../../../../hx6538_video_bridge/include"', b'')
            transform = 'remove unused private bridge include directory'
        elif target in ('components/hal/hal_audio/idf_component.yml',
                        'components/hal/hal_camera/idf_component.yml',
                        'components/protocols/mcu_link/idf_component.yml'):
            data = re.sub(rb'^url:.*$', b'url: "https://github.com/orulink-ai/WatcheRobot-public"',
                          data, flags=re.MULTILINE)
            transform = 'public repository metadata URL'
        elif target == 'components/sscma_client/test_support/host/CMakeLists.txt':
            data = (ROOT / 'tools/templates/sscma-client-host.cmake').read_bytes()
            transform = 'public host-test cJSON source path parameter and active Release assertions'
        elif target == 'examples/hardware_check/README.md':
            data = ('# Hardware check / 硬件体检\n\n'
                    'Current public instructions / 当前公开操作指南：\n\n'
                    '- [English: 26-item check](../../docs/hardware-check.md)\n'
                    '- [中文：26 项体检](../../docs/hardware-check_zh.md)\n'
                    '- [Build, flash and recovery / 构建、烧录与恢复](../../docs/getting-started.md)\n\n'
                    'Current servos have no physical position feedback; clear the entire travel '\
                    'before confirming X90/Y120. ACK/DONE is not measured position.\n').encode()
            transform = 'replace stale feedback-dependent upstream instructions with bilingual public guides'
        elif target == 'AUDIT.md':
            data = data.replace(b'../../firmware/s3/components/', b'components/')
            transform = 'public component links'
        dest = sdk / target
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        inventory.append(dict(source=path, destination=target, source_sha256=original_hash,
                              sha256=hashlib.sha256(data).hexdigest(), transform=transform))
    for relative, data in preserved.items():
        dest = sdk / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
    (sdk / 'SOURCE.json').write_text(json.dumps(dict(source_commit=ref,
        source_repository='orulink-ai/WatcheRobot_esp32', files=inventory), indent=2) + '\n')
    print(f'Exported {len(inventory)} allowlisted files from {ref}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--ref', required=True)
    args = parser.parse_args()
    export(args.source, args.ref)
