"""Prepare private flashing tools, then delegate to OpenOCD or the PTL package."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

CACHE = Path(__file__).resolve().parents[1] / '.local' / 'flash-runtime'


def check_environment():
    prefix = os.environ.get('CONDA_PREFIX')
    name = os.environ.get('CONDA_DEFAULT_ENV', '')
    if (not prefix or not name or name == 'base'
            or Path(prefix).resolve() != Path(sys.prefix).resolve()
            or not (Path(prefix) / 'conda-meta').is_dir()):
        raise RuntimeError('Create a dedicated environment: conda create -n watcherobot python=3.12 pip -y, then conda activate watcherobot. Do not use base.')
    if sys.version_info < (3, 12):
        raise RuntimeError('The flashing environment requires Python 3.12 or newer.')


def install_dependencies(requirements):
    check_environment()
    clean = os.environ.copy()
    for key in list(clean):
        if key.startswith('PIP_') or key in ('PYTHONPATH', 'PYTHONHOME', 'PYTHONUSERBASE'):
            clean.pop(key)
    clean['PIP_CONFIG_FILE'] = os.devnull
    clean['PYTHONNOUSERSITE'] = '1'
    run([sys.executable, '-I', '-m', 'pip', 'install', '--no-user', '--cache-dir', str(CACHE / 'pip-cache'), '-r', str(requirements)], env=clean)
OPENOCD = {
    ('Windows', 'x64'): ('win32-x64.zip', '6bfd3c97135aafef8affc9af1acf34fd0e2b9ca26044506f6abd7f95b7630052'),
    ('Darwin', 'arm64'): ('darwin-arm64.tar.gz', '667342c086984f3e5a55b4e0d5f711add13fb04de040fca493303000e6c19327'),
    ('Darwin', 'x64'): ('darwin-x64.tar.gz', '668ad25350103a4357e11629ec833eae5982e973889ce25bad0c2963e37fa8bf'),
    ('Linux', 'arm64'): ('linux-arm64.tar.gz', 'db73a3ab91c556ecec2405a7e02d404b11139df6aba1031cad94a7e6766d06cc'),
    ('Linux', 'x64'): ('linux-x64.tar.gz', '94b3790983beaf8ed57e646c0620dd66d705fddae03d290823a6ed3b439468d6'),
}


def run(command, **kwargs):
    print(subprocess.list2cmdline([str(x) for x in command]), flush=True)
    return subprocess.run(command, check=True, **kwargs)


def download(url, target, digest=None):
    target.parent.mkdir(parents=True, exist_ok=True)
    partial = target.with_suffix(target.suffix + '.partial')
    request = urllib.request.Request(url, headers={'User-Agent': 'WatcheRobot-flasher'})
    with urllib.request.urlopen(request, timeout=45) as source, partial.open('wb') as dest:
        shutil.copyfileobj(source, dest)
    if digest and hashlib.sha256(partial.read_bytes()).hexdigest() != digest:
        raise ValueError('Download integrity check failed: ' + target.name)
    partial.replace(target)


def extract(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as bundle:
            for name in bundle.namelist():
                if not (destination / name).resolve().is_relative_to(destination.resolve()):
                    raise ValueError('Archive path escapes destination')
            bundle.extractall(destination)
    else:
        with tarfile.open(archive) as bundle:
            bundle.extractall(destination, filter='data')


def openocd():
    existing = shutil.which('openocd')
    if existing:
        return Path(existing)
    system = platform.system()
    arch = 'arm64' if platform.machine().lower() in ('arm64', 'aarch64') else 'x64'
    suffix, digest = OPENOCD[(system, arch)]
    name = 'xpack-openocd-0.12.0-7-' + suffix
    folder = CACHE / 'openocd'
    executable = folder / 'xpack-openocd-0.12.0-7' / 'bin' / ('openocd.exe' if system == 'Windows' else 'openocd')
    if not executable.exists():
        archive = CACHE / name
        download('https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v0.12.0-7/' + name, archive, digest)
        extract(archive, folder)
    run([str(executable), '--version'])
    return executable


def windows_driver():
    if platform.system() != 'Windows':
        return
    query = "@(Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match 'VID_0483&PID_3748' } | Select-Object Status) | ConvertTo-Json -Compress"
    result = run(['powershell', '-NoProfile', '-Command', query], capture_output=True, text=True)
    devices = json.loads(result.stdout or '[]')
    if isinstance(devices, dict):
        devices = [devices]
    if not devices:
        raise RuntimeError('Connect ST-LINK V2 to USB before running STM32 flashing.')
    if len(devices) != 1:
        raise RuntimeError('Connect only the intended ST-LINK V2 before flashing.')
    if devices[0]['Status'] == 'OK':
        return
    print('Preparing the official ST-LINK Windows driver...', flush=True)
    archive = CACHE / 'stsw-link009.zip'
    try:
        download('https://www.st.com/resource/en/driver/stsw-link009.zip', archive)
    except OSError:
        print('ST download unavailable; trying the pinned ST Community attachment.', flush=True)
        download('https://community.st.com/topic/trackAttachment?file_uuid=6a4a63e8-6ed8-9a75-9771-2fb58d5226f3&redirect=1', archive,
                 'df015c7760f974e9da0f4c5a098d62c72157ea45cd0e80694eb47ab7ef28352b')
    folder = CACHE / 'stlink-driver'
    extract(archive, folder)
    infs = list(folder.rglob('stlink_dbg_winusb.inf'))
    if not infs:
        raise RuntimeError('The downloaded ST-LINK driver package contains no INF files.')
    for inf in infs:
        print('Windows may ask to trust STMicroelectronics device software. Review the system prompt; do not disable signature enforcement.', flush=True)
        run(['pnputil', '/add-driver', str(inf), '/install'])
    result = run(['powershell', '-NoProfile', '-Command', query], capture_output=True, text=True)
    devices = json.loads(result.stdout or '[]')
    if isinstance(devices, dict):
        devices = [devices]
    if len(devices) != 1 or devices[0]['Status'] != 'OK':
        raise RuntimeError('ST-LINK driver is not ready. Reconnect ST-LINK and retry; use an administrator terminal if driver installation was denied.')


def stm32(args):
    firmware = args.package.resolve() / 'watcheRobot_STM32.bin'
    if not firmware.is_file() or not firmware.stat().st_size:
        raise ValueError('Select the extracted folder containing watcheRobot_STM32.bin')
    tool = openocd()
    base = [str(tool), '-f', 'interface/stlink.cfg', '-f', 'target/stm32f1x.cfg']
    if args.prepare_only:
        run([*base, '-c', 'echo STM32_CONFIG_PARSE_OK; shutdown'])
        print('Tools ready. No device was accessed or flashed.')
        return
    windows_driver()
    # Fixed relative filename avoids Tcl injection from user-selected paths.
    try:
        run([*base, '-c', 'program watcheRobot_STM32.bin verify reset exit 0x08000000'], cwd=firmware.parent)
    except subprocess.CalledProcessError:
        print('STM32 was not verified. If OpenOCD cannot connect to the target, check board power and SIO/SWDIO, SCK/SWCLK, GND wiring. Disconnect power before changing wires; do not erase as a workaround.', file=sys.stderr)
        raise


def head(args):
    package = args.package.resolve()
    entry = package / 'tools' / 'ptl_release.py'
    requirements = package / 'requirements.txt'
    if not entry.is_file() or not requirements.is_file():
        raise ValueError('Select the extracted PTL-paired folder containing tools and requirements.txt')
    install_dependencies(requirements)
    python = sys.executable
    run([str(python), str(entry), 'verify', '--bundle', str(package)])
    if args.prepare_only:
        print('Tools ready. No device was accessed or flashed.')
        return
    if not args.port or not args.vision_port:
        run([str(python), '-m', 'serial.tools.list_ports', '-v'])
        if not args.port and not args.vision_port:
            print('Ports listed; no firmware written. Run again with --port (SERIAL-B) and --vision-port (SERIAL-A).')
            return
        raise ValueError('Specify --port (SERIAL-B) and --vision-port (SERIAL-A) from the same CH342 device.')
    run([str(python), str(entry), 'flash', '--bundle', str(package), '--port', args.port, '--vision-port', args.vision_port])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=['stm32', 'head'])
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--port')
    parser.add_argument('--vision-port')
    parser.add_argument('--prepare-only', action='store_true', help='Prepare tools without accessing hardware')
    args = parser.parse_args()
    try:
        check_environment()
        (stm32 if args.target == 'stm32' else head)(args)
        return 0
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as exc:
        print('Flashing stopped: ' + str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
