from pathlib import Path
from unittest.mock import patch

import pytest

from tools import install_sd_card_resources as module
from tools.sd_package import InstallError, Package


def test_common_fat32_names_are_normalized():
    for name in ('FAT32', 'vfat', 'MS-DOS FAT32', 'msdos'):
        assert module._normalize_filesystem(name) == 'FAT32'


def test_non_fat_filesystem_is_not_hidden():
    assert module._normalize_filesystem('ext4') == 'EXT4'


def test_failed_extraction_preserves_existing_official_assets(tmp_path: Path):
    watche = tmp_path / 'watche'
    current = watche / 'official' / 'current'
    current.mkdir(parents=True)
    (current / 'resource_manifest.json').write_text(
        '{"layout_revision": 2, "bundle_version": "v0.0.1"}', encoding='utf-8'
    )
    old_asset = watche / 'assets' / 'anim' / ('a' * 64 + '.animpack')
    old_asset.parent.mkdir(parents=True)
    old_asset.write_bytes(b'old-resource')

    package = Package('v0.0.2', tmp_path / 'package.tar.gz', 1, '0' * 64, 'test', 1, 1, 1)
    plan = module.ArchivePlan(
        'v0.0.2',
        1,
        1,
        {'files': [], 'bundle_sha256': module._calculate_bundle_hash({})},
        '0' * 64,
    )
    drive = module.DriveInfo(tmp_path, 'TEST', 'FAT32', 64 * 1024 * 1024, 128 * 1024 * 1024, 'test')

    with patch.object(module, 'validate_drive', return_value=drive), \
            patch.object(module, 'inspect_archive', return_value=plan), \
            patch.object(module, '_extract_archive', side_effect=InstallError('injected write failure')):
        with pytest.raises(InstallError, match='injected write failure'):
            module.install_package_to_card(package, drive, False)

    assert old_asset.read_bytes() == b'old-resource'
    assert current.is_dir()
