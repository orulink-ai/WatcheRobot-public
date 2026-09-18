from tools import install_sd_card_resources as module


def test_common_fat32_names_are_normalized():
    for name in ('FAT32', 'vfat', 'MS-DOS FAT32', 'msdos'):
        assert module._normalize_filesystem(name) == 'FAT32'


def test_non_fat_filesystem_is_not_hidden():
    assert module._normalize_filesystem('ext4') == 'EXT4'
