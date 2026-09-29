from pathlib import Path
import subprocess
from types import SimpleNamespace
from unittest.mock import Mock, patch

import pytest

from tools import format_sd_card as module


def drive(kind='removable', filesystem='FAT32', allocation=512):
    return SimpleNamespace(
        root=Path('F:\\'),
        label='SD',
        filesystem=filesystem,
        allocation_unit_bytes=allocation,
        total_bytes=128 * 1024 * 1024,
        kind=kind,
    )


def test_refuses_non_removable_target():
    with pytest.raises(module.FormatError, match='not reported as removable'):
        module.validate_format_target(drive(kind='explicit-fixed'))


def test_non_removable_target_never_runs_formatter():
    runner = Mock()
    with patch.object(module.platform, 'system', return_value='Windows'), \
            patch.object(module, 'inspect_explicit_drive', return_value=drive(kind='explicit-fixed')):
        with pytest.raises(module.FormatError, match='not reported as removable'):
            module.format_card(Path('F:\\'), runner=runner)
    runner.assert_not_called()


def test_formats_fat32_with_512_byte_allocation_unit():
    runner = Mock(return_value=SimpleNamespace(returncode=0))
    with patch.object(module.platform, 'system', return_value='Windows'), \
            patch.object(module, 'inspect_explicit_drive', return_value=drive()), \
            patch.object(module.time, 'sleep'), \
            patch('builtins.input', side_effect=AssertionError('Unexpected confirmation prompt')):
        result = module.format_card(Path('F:\\'), runner=runner)
    runner.assert_called_once()
    command = runner.call_args.args[0]
    assert 'Format-Volume' in command[-1]
    assert '-FileSystem FAT32' in command[-1]
    assert '-AllocationUnitSize 512' in command[-1]
    assert '-Confirm:$false' in command[-1]
    assert result.allocation_unit_bytes == 512


def test_changed_target_never_runs_formatter():
    runner = Mock()
    changed = drive()
    changed.total_bytes *= 2
    with patch.object(module.platform, 'system', return_value='Windows'), \
            patch.object(module, 'inspect_explicit_drive', side_effect=[drive(), changed]):
        with pytest.raises(module.FormatError, match='changed before formatting'):
            module.format_card(Path('F:\\'), runner=runner)
    runner.assert_not_called()


def test_native_failure_is_not_reported_as_success():
    runner = Mock(side_effect=subprocess.CalledProcessError(1, 'format'))
    with patch.object(module.platform, 'system', return_value='Windows'), \
            patch.object(module, 'inspect_explicit_drive', return_value=drive()):
        with pytest.raises(module.FormatError, match='could not format'):
            module.format_card(Path('F:\\'), runner=runner)


def test_wrong_result_fails_verification():
    runner = Mock()
    with patch.object(module.platform, 'system', return_value='Windows'), \
            patch.object(module, 'inspect_explicit_drive', return_value=drive(allocation=4096)), \
            patch.object(module.time, 'sleep'):
        with pytest.raises(module.FormatError, match='verification failed'):
            module.format_card(Path('F:\\'), runner=runner)


def test_non_windows_never_runs_formatter():
    runner = Mock()
    with patch.object(module.platform, 'system', return_value='Darwin'):
        with pytest.raises(module.FormatError, match='Windows only'):
            module.format_card(Path('/Volumes/WATCHE'), runner=runner)
    runner.assert_not_called()
