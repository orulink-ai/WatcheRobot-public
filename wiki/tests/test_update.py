import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

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


def wiki_release():
    source = 'a' * 40
    name = f'watcherobot-wiki-{source}-123-1.tar.gz'
    manifest = {'repository': module.REPOSITORY, 'sourceRef': source,
                'archive': name, 'sha256': hashlib.sha256(bundle(source)).hexdigest()}
    return {'id': 1, 'tag_name': module.WIKI_TAG, 'draft': False, 'prerelease': True,
            'body': json.dumps(manifest), 'assets': [{'name': name,
            'browser_download_url': f'https://github.com/{module.REPOSITORY}/releases/download/{module.WIKI_TAG}/{name}'}]}


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

    def test_only_complete_independent_wiki_publication_is_selected(self):
        release = wiki_release()
        self.assertIsNotNone(module.release_assets(release))
        release['assets'] = []
        self.assertIsNone(module.release_assets(release))
        release = wiki_release()
        release['draft'] = True
        self.assertIsNone(module.release_assets(release))
        release['draft'] = False
        release['tag_name'] = 'watche-system-v0.2.8'
        self.assertIsNone(module.release_assets(release))
        release = wiki_release()
        release['prerelease'] = False
        self.assertIsNone(module.release_assets(release))

    def test_wrong_asset_destination_is_rejected(self):
        release = wiki_release()
        release['assets'][0]['browser_download_url'] = 'https://example.com/evil'
        with self.assertRaises(ValueError):
            module.release_assets(release)

    def test_invalid_publication_identity_is_rejected(self):
        for field, value in [('repository', 'other/repo'), ('sourceRef', 'main'),
                             ('archive', '../other.tar.gz'), ('sha256', 'bad')]:
            release = wiki_release()
            manifest = json.loads(release['body'])
            manifest[field] = value
            release['body'] = json.dumps(manifest)
            with self.subTest(field=field), self.assertRaises(ValueError):
                module.release_assets(release)

    def test_first_publication_pending_keeps_current(self):
        release = wiki_release()
        release['body'] = 'Documentation publication pending.'
        self.assertIsNone(module.release_assets(release))

    def test_run_uses_dedicated_channel_and_exact_commit(self):
        release = wiki_release()
        publication = json.loads(release['body'])
        payload = bundle(publication['sourceRef'])
        publication['sha256'] = hashlib.sha256(payload).hexdigest()
        release['body'] = json.dumps(publication)
        def download(url, limit=module.MAX_DOWNLOAD):
            if url.endswith('/releases/tags/wiki-site'):
                return json.dumps(release).encode()
            if url.endswith('.tar.gz'):
                return payload
            if url.endswith('/version.json'):
                return json.dumps({'repository': module.REPOSITORY,
                                   'sourceRef': publication['sourceRef']}).encode()
            if url == 'https://wiki.orulink.ai/':
                return b'<html>Wiki</html>'
            self.fail(f'Unexpected download: {url}')
        with patch.object(module, 'download', side_effect=download), patch.object(module, 'install_bundle') as install:
            module.run(self.root, 'https://wiki.orulink.ai/')
            args = install.call_args.args
            self.assertEqual(args[1:5], (payload, publication['sha256'], publication['sourceRef'], 1))
            args[5]()

    def test_incomplete_publication_does_not_install(self):
        release = wiki_release()
        release['assets'] = []
        with patch.object(module, 'download', return_value=json.dumps(release).encode()), patch.object(module, 'install_bundle') as install:
            module.run(self.root, 'https://wiki.orulink.ai/')
            install.assert_not_called()


if __name__ == '__main__':
    unittest.main()
