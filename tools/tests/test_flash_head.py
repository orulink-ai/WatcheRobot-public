import json
from pathlib import Path
import subprocess
from types import SimpleNamespace
from unittest.mock import Mock

import pytest
from tools import flash_head as module


@pytest.fixture
def package(tmp_path):
    (tmp_path / 'provenance.json').write_text(json.dumps({
        'profile': 'ptl', 'layout': {'0x0': 'bootloader.bin', '0x20000': 'WatcheRobot-S3.bin',
                                   '0xd20000': 'storage.bin'}}))
    return tmp_path


def execute(package, failure=None, factory=False):
    commands = []
    def runner(command, **kwargs):
        commands.append(command)
        if failure:
            failure(command)
        return SimpleNamespace(stdout='power ready\n')
    module.flash(package, 'B', 'A', factory=factory, runner=runner,
                 sleep=lambda _: None, preflight=lambda *_: None)
    return commands


def test_fixed_order_and_one_write_per_chip(package):
    commands = execute(package, factory=True)
    assert 'verify' in commands[0]
    assert 'write-flash' in commands[1]
    assert str(package / 'esp32/storage.bin') in commands[1]
    assert 'erase-region' in commands[2]
    assert 'prepare-flash' in commands[3]
    assert Path(commands[4][1]).name == 'flash_hx_uart.py'
    assert 'exit' in commands[5]
    assert len(commands) == 6


def test_application_update_never_writes_storage(package):
    commands = execute(package)
    assert not any('storage.bin' in word for word in commands[1])


@pytest.mark.parametrize('step', ['verify', 'write-flash', 'erase-region'])
def test_early_failure_does_not_start_himax(package, step):
    commands = []
    def runner(command, **kwargs):
        commands.append(command)
        if step in command:
            raise subprocess.CalledProcessError(1, command)
    with pytest.raises(RuntimeError):
        module.flash(package, 'B', 'A', runner=runner, preflight=lambda *_: None,
                     sleep=lambda _: None)
    assert not any('prepare-flash' in command for command in commands)


def test_uninitialized_power_retries_without_reflashing_esp32(package):
    attempts = 0
    def failure(command):
        nonlocal attempts
        if 'prepare-flash' in command:
            attempts += 1
            if attempts < 3:
                raise subprocess.CalledProcessError(1, command, stderr=module.POWER_NOT_INITIALIZED)
    commands = execute(package, failure)
    assert attempts == 3
    assert sum('write-flash' in command for command in commands) == 1


@pytest.mark.parametrize('error', [module.POWER_NOT_INITIALIZED, 'I2C failure'])
def test_power_failure_stops_and_exits_maintenance(package, error):
    commands = []
    def runner(command, **kwargs):
        commands.append(command)
        if 'prepare-flash' in command:
            raise subprocess.CalledProcessError(1, command, stderr=error)
        return SimpleNamespace(stdout='')
    with pytest.raises(RuntimeError, match='power initialization'):
        module.flash(package, 'B', 'A', runner=runner, preflight=lambda *_: None,
                     sleep=lambda _: None)
    assert sum('prepare-flash' in c for c in commands) == (3 if error == module.POWER_NOT_INITIALIZED else 1)
    assert 'exit' in commands[-1]
    assert not any(Path(c[1]).name == 'flash_hx_uart.py' for c in commands)


def test_himax_failure_cleans_up_without_success(package, capsys):
    commands = []
    def runner(command, **kwargs):
        commands.append(command)
        if Path(command[1]).name == 'flash_hx_uart.py':
            raise subprocess.CalledProcessError(1, command)
        return SimpleNamespace(stdout='')
    with pytest.raises(RuntimeError, match='Himax flashing'):
        module.flash(package, 'B', 'A', runner=runner, preflight=lambda *_: None,
                     sleep=lambda _: None)
    assert 'exit' in commands[-1]
    assert 'PTL paired flash completed.' not in capsys.readouterr().out


def test_busy_port_prevents_writes(package):
    runner = Mock()
    with pytest.raises(RuntimeError, match='checks'):
        module.flash(package, 'B', 'A', runner=runner,
                     preflight=Mock(side_effect=OSError('port busy')))
    assert runner.call_count == 1


def test_power_timeout_cleans_up_without_starting_himax(package):
    commands = []
    def runner(command, **kwargs):
        commands.append(command)
        if 'prepare-flash' in command:
            raise subprocess.TimeoutExpired(command, 30)
        return SimpleNamespace(stdout='')
    with pytest.raises(RuntimeError, match='power initialization'):
        module.flash(package, 'B', 'A', runner=runner, preflight=lambda *_: None,
                     sleep=lambda _: None)
    assert 'exit' in commands[-1]
    assert sum('prepare-flash' in c for c in commands) == 1


def test_restart_failure_never_reports_success(package, capsys):
    def failure(command):
        if 'exit' in command:
            raise subprocess.CalledProcessError(1, command)
    with pytest.raises(RuntimeError, match='during restart'):
        execute(package, failure)
    assert 'PTL paired flash completed.' not in capsys.readouterr().out


@pytest.mark.parametrize('step', ['write-flash', 'flash_hx_uart.py'])
def test_write_timeout_stops_sequence(package, step, capsys):
    commands = []
    def runner(command, **kwargs):
        commands.append(command)
        assert 0 < kwargs['timeout'] <= 900
        if step in command or Path(command[1]).name == step:
            raise subprocess.TimeoutExpired(command, kwargs['timeout'])
        return SimpleNamespace(stdout='')
    with pytest.raises(RuntimeError, match='Head flash stopped'):
        module.flash(package, 'B', 'A', runner=runner, preflight=lambda *_: None,
                     sleep=lambda _: None)
    if step == 'write-flash':
        assert not any('prepare-flash' in c for c in commands)
    else:
        assert 'exit' in commands[-1]
    assert 'PTL paired flash completed.' not in capsys.readouterr().out


def test_non_paired_package_rejected_before_ports(package):
    (package / 'provenance.json').write_text('{"profile":"esp32"}')
    preflight = Mock()
    runner = Mock()
    with pytest.raises(RuntimeError, match='complete PTL-paired'):
        module.flash(package, 'B', 'A', runner=runner, preflight=preflight)
    preflight.assert_not_called()
    assert runner.call_count == 1
