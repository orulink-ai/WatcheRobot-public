"""Run a verified paired release in ESP32-first order without interactive prompts."""
import argparse
from contextlib import ExitStack
import json
from pathlib import Path
import subprocess
import sys
import time

POWER_NOT_INITIALIZED = 'Himax power pin is not configured as an output'


def run(command, **kwargs):
    print(subprocess.list2cmdline([str(item) for item in command]), flush=True)
    return subprocess.run(command, check=True, **kwargs)


def check_ports(control, vision):
    from flash_setup import validate_head_port_roles
    import serial
    validate_head_port_roles(control, vision)
    # Open both before any write, with modem lines disabled before open.
    with ExitStack() as stack:
        for name in (vision, control):
            port = serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=2)
            port.dtr = False
            port.rts = False
            port.port = name
            stack.callback(port.close)
            port.open()


def flash(package, control, vision, factory=False, runner=run, sleep=time.sleep,
          preflight=check_ports):
    package = Path(package).resolve()
    entry = package / 'tools/ptl_release.py'
    maintenance = [sys.executable, str(package / 'tools/himax_maintenance.py')]
    ports = ['--control-port', control, '--vision-port', vision]
    stage = 'package and port checks'
    maintenance_attempted = False
    try:
        print('[1/5] Checking package and ports', flush=True)
        runner([sys.executable, str(entry), 'verify', '--bundle', str(package)])
        manifest = json.loads((package / 'provenance.json').read_text(encoding='utf-8'))
        if manifest.get('profile') != 'ptl':
            raise ValueError('Head flashing requires a complete PTL-paired package.')
        preflight(control, vision)

        stage = 'ESP32 flashing'
        print('[2/5] Flashing ESP32', flush=True)
        base = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port', control]
        if factory:
            print('Factory installation overwrites existing ESP32 data.', flush=True)
            segments = [value for offset, name in manifest['layout'].items()
                        for value in (offset, str(package / 'esp32' / name))]
        else:
            segments = ['0x20000', str(package / 'esp32/WatcheRobot-S3.bin')]
        runner([*base, '--after', 'no-reset', 'write-flash', '--flash-mode', 'dio',
                '--flash-freq', '80m', '--flash-size', '32MB', *segments])
        runner([*base, '--after', 'hard-reset', 'erase-region', '0xf000', '0x2000'])

        stage = 'ESP32 startup and Himax power initialization'
        print('[3/5] Waiting for startup and checking Himax power initialization', flush=True)
        # Each unsuccessful maintenance probe resets ESP32 in its own failure cleanup.
        # Allow a longer boot window on retry; readiness comes from register readback,
        # never from elapsed time alone. Only the known not-initialized error retries.
        for delay in (5, 10, 20):
            sleep(delay)
            maintenance_attempted = True
            try:
                result = runner([*maintenance, 'prepare-flash', *ports],
                                capture_output=True, text=True, timeout=30)
                print(result.stdout or '', end='', flush=True)
                break
            except subprocess.CalledProcessError as exc:
                detail = (exc.stdout or '') + (exc.stderr or '')
                print(detail, end='', flush=True)
                if POWER_NOT_INITIALIZED not in detail or delay == 20:
                    raise

        stage = 'Himax flashing'
        print('[4/5] Flashing Himax', flush=True)
        runner([sys.executable, str(package / 'tools/flash_hx_uart.py'),
                '--s3-port', control, '--hx-port', vision, '--reset-mode', 'hx-reset',
                '--image', str(package / 'himax/build/watcher_hx6538_webrtc_bridge.img')])
        stage = 'restart'
        print('[5/5] Himax reboot accepted; restarting ESP32', flush=True)
        runner([*maintenance, 'exit', *ports], timeout=30)
        maintenance_attempted = False
    except (Exception, KeyboardInterrupt) as exc:
        if maintenance_attempted:
            try:
                runner([*maintenance, 'exit', *ports], timeout=30)
            except (Exception, KeyboardInterrupt) as cleanup:
                print(f'Could not exit maintenance: {cleanup}. Power-cycle the head.', file=sys.stderr)
        raise RuntimeError(f'Head flash stopped during {stage}: {exc}') from exc
    print('PTL paired flash completed. Restart requested; normal operation is not yet verified.', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--vision-port', required=True)
    parser.add_argument('--factory', action='store_true')
    args = parser.parse_args()
    try:
        flash(args.bundle, args.port, args.vision_port, args.factory)
        return 0
    except (OSError, ValueError, RuntimeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
