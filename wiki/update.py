"""Pull public Wiki release assets, validate, and atomically activate. No GitHub credentials."""
import argparse
from contextlib import contextmanager
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import tarfile
import tempfile
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen

REPOSITORY = 'orulink-ai/WatcheRobot-public'
ARCHIVE = 'watcherobot-wiki.tar.gz'
CHECKSUM = ARCHIVE + '.sha256'
MAX_DOWNLOAD = 64 * 1024 * 1024
MAX_EXPANDED = 256 * 1024 * 1024


def download(url, limit=MAX_DOWNLOAD):
    request = Request(url, headers={'User-Agent':'WatcheRobot-Wiki-Updater', 'Accept':'application/vnd.github+json'})
    with urlopen(request, timeout=60) as response:
        content = response.read(limit + 1)
    if len(content) > limit:
        raise ValueError('Download exceeds size limit')
    return content


def release_assets(release):
    if release.get('draft') or release.get('prerelease'):
        return None
    assets = {asset['name']:asset for asset in release.get('assets', [])}
    if not all(name in assets for name in (ARCHIVE, CHECKSUM)):
        return None
    prefix = f"https://github.com/{REPOSITORY}/releases/download/{quote(release['tag_name'], safe='')}/"
    for name in (ARCHIVE, CHECKSUM):
        if assets[name]['browser_download_url'] != prefix + name:
            raise ValueError('Unexpected release asset destination')
    return assets


@contextmanager
def update_lock(root):
    root.mkdir(parents=True, exist_ok=True)
    with (root / '.update.lock').open('a+b') as lock:
        if os.name == 'nt':
            import msvcrt
            lock.write(b'0')
            lock.flush()
            lock.seek(0)
            msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        try:
            yield
        finally:
            if os.name == 'nt':
                lock.seek(0)
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)


def activate(root, target):
    temporary = root / '.current.next'
    temporary.unlink(missing_ok=True)
    temporary.symlink_to(target, target_is_directory=True)
    os.replace(temporary, root / 'current')


def install_bundle(root, payload, digest, tag, release_id, health):
    root = Path(root).resolve()
    if not re.fullmatch(r'[a-f0-9]{64}', digest) or hashlib.sha256(payload).hexdigest() != digest:
        raise ValueError('SHA256 mismatch')
    if not isinstance(release_id, int) or release_id < 1:
        raise ValueError('Invalid release id')
    releases = root / 'releases'
    releases.mkdir(parents=True, exist_ok=True)
    current = root / 'current'
    if current.exists() and not current.is_symlink():
        raise ValueError('current must be a symlink')
    previous = os.readlink(current) if current.is_symlink() else None
    name = f'{release_id}-{digest[:16]}'
    target = 'releases/' + name
    destination = releases / name
    if previous == target or (current.is_symlink() and current.resolve() == destination):
        return False
    with tempfile.TemporaryDirectory(prefix='.staging-', dir=releases) as staging:
        stage = Path(staging)
        with tarfile.open(fileobj=io.BytesIO(payload), mode='r:gz') as archive:
            total = 0
            for count, member in enumerate(archive, start=1):
                path = PurePosixPath(member.name)
                total += member.size
                if (count > 20000 or total > MAX_EXPANDED or path.is_absolute()
                        or '..' in path.parts or '\\' in member.name or ':' in member.name
                        or not (member.isfile() or member.isdir())):
                    raise ValueError('Unsafe or oversized archive')
                output = stage.joinpath(*path.parts)
                if member.isdir():
                    output.mkdir(parents=True, exist_ok=True)
                else:
                    output.parent.mkdir(parents=True, exist_ok=True)
                    with archive.extractfile(member) as source, output.open('xb') as sink:
                        shutil.copyfileobj(source, sink)
                    output.chmod(0o644)
        manifest = json.loads((stage / 'version.json').read_text())
        if manifest.get('repository') != REPOSITORY or manifest.get('sourceRef') != tag:
            raise ValueError('Release identity mismatch')
        for page in ('index.html', 'en/index.html'):
            if not (stage / page).is_file() or (stage / page).stat().st_size == 0:
                raise ValueError('Missing homepage')
        stage.chmod(0o755)
        # A failed health check may leave this immutable directory for retry.
        if destination.exists():
            shutil.rmtree(destination)
        os.replace(stage, destination)
    activate(root, target)
    try:
        health()
    except Exception:
        if previous is None:
            current.unlink()
        else:
            activate(root, previous)
        raise
    return True


def run(root, health_url):
    with update_lock(root):
        try:
            release = json.loads(download(f'https://api.github.com/repos/{REPOSITORY}/releases/latest', 1024 * 1024))
        except HTTPError as error:
            if error.code == 404:
                print('No published stable Release yet; keeping current Wiki')
                return
            raise
        assets = release_assets(release)
        if assets is None:
            print('Latest Release has no complete Wiki assets yet; keeping current Wiki')
            return
        checksum = download(assets[CHECKSUM]['browser_download_url'], 1024).decode('ascii').strip()
        match = re.fullmatch(r'([a-f0-9]{64})\s+\*?' + re.escape(ARCHIVE), checksum)
        if not match:
            raise ValueError('Invalid checksum file')
        digest = match[1]
        target = f"releases/{release['id']}-{digest[:16]}"
        if (root / 'current').is_symlink() and os.readlink(root / 'current') == target:
            print(f"Wiki already current: {release['tag_name']}")
            return
        payload = download(assets[ARCHIVE]['browser_download_url'])
        def health():
            version = json.loads(download(health_url.rstrip('/') + '/version.json', 4096))
            if version.get('sourceRef') != release['tag_name']:
                raise RuntimeError('Live Wiki version mismatch')
            if b'<html' not in download(health_url, 4 * 1024 * 1024).lower():
                raise RuntimeError('Live homepage unavailable')
        install_bundle(root, payload, digest, release['tag_name'], release['id'], health)
        print(f"Wiki updated to {release['tag_name']}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--health-url', default='https://wiki.orulink.ai/')
    options = parser.parse_args()
    run(options.root.resolve(), options.health_url)
