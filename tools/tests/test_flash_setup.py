import importlib.util
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import subprocess
import zipfile
import os

import pytest

spec = importlib.util.spec_from_file_location('flash_setup', Path(__file__).parents[1] / 'flash_setup.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


@pytest.mark.parametrize('prefix', [None, 'base', 'some-other-env'])
def test_rejects_existing_environments(prefix):
    with patch.dict(os.environ, {}, clear=True):
        if prefix:
            os.environ['CONDA_PREFIX'] = prefix
        with pytest.raises(RuntimeError):
            module.check_environment()


def test_requires_matching_python(tmp_path):
    with patch.dict(os.environ, {'CONDA_PREFIX': str(tmp_path), 'CONDA_DEFAULT_ENV': 'watcherobot'}), patch.object(module.sys, 'prefix', 'another-python'):
        with pytest.raises(RuntimeError):
            module.check_environment()


def test_accepts_dedicated_named_environment(tmp_path):
    (tmp_path / 'conda-meta').mkdir()
    with patch.dict(os.environ, {'CONDA_PREFIX': str(tmp_path), 'CONDA_DEFAULT_ENV': 'watcherobot-flash-test'}), patch.object(module.sys, 'prefix', str(tmp_path)):
        module.check_environment()


def test_pip_cannot_inherit_install_destination(tmp_path):
    with patch.object(module, 'check_environment'), patch.dict(os.environ, {'PIP_TARGET': 'outside', 'PIP_USER': '1', 'PYTHONPATH': 'outside'}), patch.object(module, 'run') as run:
        module.install_dependencies(tmp_path / 'requirements.txt')
        command = run.call_args.args[0]
        env = run.call_args.kwargs['env']
        assert command[:4] == [module.sys.executable, '-I', '-m', 'pip']
        assert '--no-user' in command
        assert 'PIP_TARGET' not in env and 'PIP_USER' not in env and 'PYTHONPATH' not in env
        assert env['PIP_CONFIG_FILE'] == os.devnull


def test_missing_firmware_never_prepares_or_opens_hardware(tmp_path):
    with patch.object(module, 'openocd') as tool:
        with pytest.raises(ValueError):
            module.stm32(SimpleNamespace(package=tmp_path, prepare_only=False))
        tool.assert_not_called()


def test_prepare_does_not_install_driver_or_flash(tmp_path):
    (tmp_path / 'watcheRobot_STM32.bin').write_bytes(b'firmware')
    with patch.object(module, 'openocd', return_value=Path('openocd')), patch.object(module, 'windows_driver') as driver, patch.object(module, 'run') as run:
        module.stm32(SimpleNamespace(package=tmp_path, prepare_only=True))
        driver.assert_not_called()
        assert all('program ' not in str(call) for call in run.call_args_list)


def test_driver_failure_blocks_flash(tmp_path):
    (tmp_path / 'watcheRobot_STM32.bin').write_bytes(b'firmware')
    with patch.object(module, 'openocd', return_value=Path('openocd')), patch.object(module, 'windows_driver', side_effect=RuntimeError('driver missing')), patch.object(module, 'run') as run:
        with pytest.raises(RuntimeError):
            module.stm32(SimpleNamespace(package=tmp_path, prepare_only=False))
        run.assert_not_called()


def test_nonzero_flash_result_is_not_swallowed(tmp_path):
    (tmp_path / 'watcheRobot_STM32.bin').write_bytes(b'firmware')
    with patch.object(module, 'openocd', return_value=Path('openocd')), patch.object(module, 'windows_driver'), patch.object(module, 'run', side_effect=subprocess.CalledProcessError(1, ['openocd'])):
        with pytest.raises(subprocess.CalledProcessError):
            module.stm32(SimpleNamespace(package=tmp_path, prepare_only=False))


def test_zip_path_escape_is_rejected(tmp_path):
    archive = tmp_path / 'bad.zip'
    with zipfile.ZipFile(archive, 'w') as bundle:
        bundle.writestr('../outside.txt', 'bad')
    with pytest.raises(ValueError):
        module.extract(archive, tmp_path / 'out')
    assert not (tmp_path / 'outside.txt').exists()


def test_listing_ports_does_not_flash(tmp_path):
    (tmp_path / 'tools').mkdir()
    (tmp_path / 'tools/ptl_release.py').touch()
    (tmp_path / 'requirements.txt').touch()
    with patch.object(module, 'install_dependencies'), \
            patch.object(module, 'print_head_port_mapping') as mapping, \
            patch.object(module, 'run') as run:
        module.head(SimpleNamespace(package=tmp_path, prepare_only=False, port=None, vision_port=None))
        assert all('flash' not in call.args[0] for call in run.call_args_list)
        mapping.assert_called_once_with()


def head_port(device, description, serial='WATCHER'):
    return SimpleNamespace(device=device, description=description, vid=0x1A86,
                           pid=0x55D2, serial_number=serial)


def test_head_port_mapping_prints_arguments(capsys):
    ports = [head_port('COM7', 'USB-Enhanced-SERIAL-A CH342'),
             head_port('COM8', 'USB-Enhanced-SERIAL-B CH342')]
    module.print_head_port_mapping(ports)
    output = capsys.readouterr().out
    assert 'SERIAL-B' in output and '--port COM8' in output
    assert 'SERIAL-A' in output and '--vision-port COM7' in output


def test_accepts_confirmed_head_port_roles():
    ports = [head_port('COM7', 'USB-Enhanced-SERIAL-A CH342'),
             head_port('COM8', 'USB-Enhanced-SERIAL-B CH342')]
    module.validate_head_port_roles('COM8', 'COM7', ports)


def test_reversed_head_ports_are_rejected_with_correct_mapping():
    ports = [head_port('COM7', 'USB-Enhanced-SERIAL-A CH342'),
             head_port('COM8', 'USB-Enhanced-SERIAL-B CH342')]
    with pytest.raises(ValueError, match=r'--port COM8.*--vision-port COM7'):
        module.validate_head_port_roles('COM7', 'COM8', ports)


def test_factory_flag_is_forwarded_only_after_role_validation(tmp_path):
    (tmp_path / 'tools').mkdir()
    (tmp_path / 'tools/ptl_release.py').touch()
    (tmp_path / 'requirements.txt').touch()
    args = SimpleNamespace(package=tmp_path, prepare_only=False, port='COM8',
                           vision_port='COM7', factory=True)
    with patch.object(module, 'install_dependencies'), \
            patch.object(module, 'validate_head_port_roles'), \
            patch.object(module, 'run') as run:
        module.head(args)
    assert run.call_args.args[0][-1] == '--factory'


def test_driver_fallback_is_pinned_and_rechecked(tmp_path):
    folder = tmp_path / 'stlink-driver'
    folder.mkdir()
    (folder / 'stlink_dbg_winusb.inf').touch()
    before = SimpleNamespace(stdout='{"Status":"Error"}')
    after = SimpleNamespace(stdout='{"Status":"OK"}')
    with patch.object(module.platform, 'system', return_value='Windows'), patch.object(module, 'CACHE', tmp_path), patch.object(module, 'extract'), patch.object(module, 'download', side_effect=[OSError('timeout'), None]) as download, patch.object(module, 'run', side_effect=[before, None, after]):
        module.windows_driver()
        assert len(download.call_args.args[2]) == 64


def test_sd_writer_downloads_latest_when_package_is_omitted():
    args = SimpleNamespace(package=None, drive=Path('E:/'), force=False,
                           prepare_only=False)
    with patch.object(module, 'run') as run:
        module.sd_card(args)
    command = run.call_args.args[0]
    assert command[:2] == [module.sys.executable,
                           str(Path(module.__file__).with_name('install_sd_card_resources.py'))]
    assert command[2:] == ['--drive', str(Path('E:/'))]


def test_sd_writer_forwards_local_archive_and_force(tmp_path):
    archive = tmp_path / 'watche-sd-resources-test.tar.gz'
    archive.touch()
    args = SimpleNamespace(package=archive, drive=Path('E:/'), force=True,
                           prepare_only=False)
    with patch.object(module, 'run') as run:
        module.sd_card(args)
    command = run.call_args.args[0]
    assert command[-3:] == ['--file', str(archive.resolve()), '--force']


def test_sd_writer_requires_explicit_card_root():
    args = SimpleNamespace(package=None, drive=None, force=False,
                           prepare_only=False)
    with pytest.raises(ValueError, match='--drive'):
        module.sd_card(args)
