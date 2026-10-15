import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('wiki_update', Path(__file__).parents[1] / 'update.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def bundle(tag='v1', extra=None):
    data = io.BytesIO()
    files = {'index.html': b'<html>Wiki</html>', 'en/index.html': b'<html>Wiki</html>',
             'version.json': json.dumps({'repository': module.REPOSITORY, 'sourceRef': tag}).encode()}
    files.update(extra or {})
    with tarfile.open(fileobj=data, mode='w:gz') as archive:
        for name, content in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(content)
            archive.addfile(info, io.BytesIO(content))
    return data.getvalue()


class WikiUpdateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def install(self, payload, tag='v1', health=lambda: None):
        return module.install_bundle(self.root, payload, hashlib.sha256(payload).hexdigest(), tag, 123, health)

    @unittest.skipIf(os.name == 'nt', 'Atomic symlink deployment is tested on the Linux server; Windows needs symlink privileges')
    def test_install_and_idempotency(self):
        payload = bundle()
        self.assertTrue(self.install(payload))
        previous = (self.root / 'current').resolve()
        self.assertFalse(self.install(payload))
        self.assertEqual((self.root / 'current').resolve(), previous)
        self.assertIn('Wiki', (self.root / 'current/index.html').read_text())

    @unittest.skipIf(os.name == 'nt', 'Requires symlink privileges; run full suite on Linux')
    def test_checksum_failure_keeps_current(self):
        self.install(bundle())
        previous = (self.root / 'current').resolve()
        with self.assertRaises(ValueError):
            module.install_bundle(self.root, bundle('v2'), '0' * 64, 'v2', 124, lambda: None)
        self.assertEqual((self.root / 'current').resolve(), previous)

    def test_archive_traversal_and_links_are_rejected(self):
        with self.assertRaises(ValueError):
            self.install(bundle(extra={'../escape': b'bad'}))
        data = io.BytesIO()
        with tarfile.open(fileobj=data, mode='w:gz') as archive:
            info = tarfile.TarInfo('index.html')
            info.type = tarfile.SYMTYPE
            info.linkname = '/etc/passwd'
            archive.addfile(info)
        with self.assertRaises(ValueError):
            self.install(data.getvalue())
        self.assertFalse((self.root.parent / 'escape').exists())

    def test_wrong_release_identity_is_rejected(self):
        with self.assertRaises(ValueError):
            self.install(bundle('different'))
        self.assertFalse((self.root / 'current').exists())

    @unittest.skipIf(os.name == 'nt', 'Requires symlink privileges; run full suite on Linux')
    def test_failed_health_check_rolls_back(self):
        self.install(bundle())
        previous = (self.root / 'current').resolve()
        def unhealthy():
            raise RuntimeError('health failed')
        with self.assertRaises(RuntimeError):
            self.install(bundle('v2'), 'v2', unhealthy)
        self.assertEqual((self.root / 'current').resolve(), previous)

    @unittest.skipIf(os.name == 'nt', 'Requires symlink privileges; run full suite on Linux')
    def test_failed_first_health_check_removes_new_link(self):
        def unhealthy():
            raise RuntimeError('health failed')
        with self.assertRaises(RuntimeError):
            self.install(bundle(), health=unhealthy)
        self.assertFalse((self.root / 'current').exists())

    def test_only_complete_stable_release_assets_are_selected(self):
        release = {'id':1, 'tag_name':'v1', 'draft':False, 'prerelease':False, 'assets':[]}
        self.assertIsNone(module.release_assets(release))
        release['assets'] = [{'name':name, 'browser_download_url':f'https://github.com/{module.REPOSITORY}/releases/download/v1/{name}'} for name in (module.ARCHIVE, module.CHECKSUM)]
        self.assertIsNotNone(module.release_assets(release))
        release['prerelease'] = True
        self.assertIsNone(module.release_assets(release))
        release['prerelease'] = False
        release['assets'][0]['browser_download_url'] = 'https://example.com/evil'
        with self.assertRaises(ValueError):
            module.release_assets(release)


if __name__ == '__main__':
    unittest.main()
