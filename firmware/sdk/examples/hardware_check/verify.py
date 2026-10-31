"""Bounded, silent hardware checks. Never accepts a human result or drives motors."""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import sys
import time

SAFE_COMMANDS = frozenset(('diag status', 'diag photo', 'diag stream', 'diag record', 'diag cancel', 'diag reset'))


def send(port, command: str) -> None:
    if command not in SAFE_COMMANDS:
        raise ValueError('Only safe hardware-check diagnostics are allowed')
    port.write((command + '\n').encode('ascii'))
    port.flush()


def result_for(line: str, attempt: int):
    match = re.search(r'attempt=(\d+) phase=(\d+) error=(\S+) cancelled=([01])', line)
    if not match or int(match[1]) != attempt:
        return None
    return int(match[2]), match[3], match[4] == '1'


class Session:
    def __init__(self, port, log):
        self.port, self.log = port, log
        self.lines: list[str] = []

    def wait(self, predicate, seconds=18):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            line = self.port.readline().decode('utf-8', errors='replace').strip()
            if not line:
                continue
            self.lines.append(line)
            if self.log:
                self.log.write(line + '\n')
                self.log.flush()
            if any(fault in line for fault in ('Guru Meditation', 'assert failed', 'Stack canary')):
                raise RuntimeError(line)
            value = predicate(line)
            if value:
                return value
        raise TimeoutError('No matching diagnostic response; check port/firmware/logs')

    def start(self, command):
        send(self.port, command)
        def began(line):
            match = re.search(r'hardware_check: begin test=\d+ item=\d+ attempt=(\d+) op=\d+', line)
            return int(match[1]) if match else None
        return self.wait(began, 5)

    def complete(self, attempt, cancelled=False):
        phase, error, actual_cancelled = self.wait(lambda line: result_for(line, attempt))
        if (phase, error, actual_cancelled) != (1 if cancelled else 4, 'ESP_OK', cancelled):
            raise RuntimeError(f'Unexpected result: attempt={attempt} phase={phase} error={error} cancelled={actual_cancelled}')

    def operation(self, command):
        attempt = self.start(command)
        self.complete(attempt)
        print(f'{command}: software evidence complete (not human acceptance)', flush=True)
        return attempt


def run(session):
    send(session.port, 'diag status')
    state = session.wait(lambda line: re.search(r'hardware_check: state modules=(\d+) body_fault=(\d+)', line), 12)
    if int(state[1]) & 7 != 7:
        raise RuntimeError('Head modules are not all ready; inspect initialization logs')
    print(f'Modules={state[1]}, body_fault={state[2]}', flush=True)
    session.operation('diag photo')
    session.operation('diag photo')  # Replacing an existing retained image.
    attempt = session.operation('diag stream')
    stream = [line for line in session.lines if f'stream attempt={attempt} ' in line]
    if not stream or not re.search(r'frames=[2-9]\d*|frames=1\d+', stream[-1]) or 'cleanup=ESP_OK' not in stream[-1]:
        raise RuntimeError('Missing live-frame or cleanup evidence')
    attempt = session.start('diag stream')
    time.sleep(0.8)
    send(session.port, 'diag cancel')
    session.complete(attempt, cancelled=True)
    print('Stream cancellation: owner cleanup completed', flush=True)
    session.operation('diag photo')  # Resource reuse after cancel/fence.
    attempt = session.operation('diag record')
    if not any('samples=48000' in line and 'complete=1' in line and f'record attempt={attempt} ' in line for line in session.lines):
        raise RuntimeError('No complete three-second PCM recording')
    send(session.port, 'diag reset')
    send(session.port, 'diag status')
    session.wait(lambda line: 'hardware_check: state ' in line and 'phase=0 ' in line, 12)
    print('Finished at welcome page. No test marked passed; no sound/lights/motion started.')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='Explicit ESP32 serial port; never auto-detected')
    parser.add_argument('--log', type=Path, help='Optional local log, never commit recordings/snapshots/logs')
    args = parser.parse_args(argv)
    import serial  # ESP-IDF environment includes pyserial; pure parser tests do not need it.
    try:
        port = serial.Serial(port=None, baudrate=115200, timeout=0.2, rtscts=False, dsrdtr=False)
        port.dtr = port.rts = False
        port.port = args.port
        port.open()
        with port:
            port.reset_input_buffer()
            if args.log:
                with args.log.open('w', encoding='utf-8') as log:
                    run(Session(port, log))
            else:
                run(Session(port, None))
    except (OSError, RuntimeError, TimeoutError, serial.SerialException) as error:
        print(f'Hardware check failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
