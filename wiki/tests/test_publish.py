import importlib.util
import json
import os
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import io

spec = importlib.util.spec_from_file_location('wiki_publish', Path(__file__).parents[1] / 'publish.py')
module = importlib.util.module_from_spec(spec)
sys.path.insert(0, str(Path(__file__).parents[1]))
try:
    spec.loader.exec_module(module)
finally:
    sys.path.pop(0)

SOURCE = 'a' * 40


class WikiPublishTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.archive = Path(self.temp.name) / 'watcherobot-wiki.tar.gz'
        with tarfile.open(self.archive, 'w:gz') as bundle:
            data = json.dumps({'repository': module.REPOSITORY, 'sourceRef': SOURCE}).encode()
            info = tarfile.TarInfo('./version.json')
            info.size = len(data)
            bundle.addfile(info, io.BytesIO(data))
        self.env = {'GITHUB_REF': 'refs/heads/main', 'GH_REPO': module.REPOSITORY,
                    'GITHUB_SHA': SOURCE, 'GITHUB_RUN_ID': '123', 'GITHUB_RUN_ATTEMPT': '1'}

    def test_manifest_has_exact_source_and_unique_asset(self):
        result = module.publication(self.archive, SOURCE, '123-1')
        self.assertEqual(result['sourceRef'], SOURCE)
        self.assertEqual(result['archive'], f'watcherobot-wiki-{SOURCE}-123-1.tar.gz')
        self.assertRegex(result['sha256'], r'^[a-f0-9]{64}$')
        with self.assertRaises(ValueError):
            module.publication(self.archive, 'b' * 40, '123-1')
        with self.assertRaises(ValueError):
            module.publication(self.archive, SOURCE, '../bad')

    def run_publish(self, api):
        original = Path.cwd()
        try:
            os.chdir(self.temp.name)
            with patch.dict(os.environ, self.env), patch.object(module, 'gh', side_effect=api) as gh:
                module.main()
                return gh
        finally:
            os.chdir(original)

    def test_upload_precedes_pointer_and_never_changes_latest(self):
        def api(*args):
            if args[0] == 'api':
                return '[[]]' if '--paginate' in args else SOURCE
            if args[:2] == ('release', 'edit'):
                manifest = json.loads(Path(args[-1]).read_text())
                self.assertEqual(manifest['sourceRef'], SOURCE)
            return ''
        gh = self.run_publish(api)
        mutations = [c.args for c in gh.call_args_list if c.args[0] == 'release']
        self.assertEqual([c[1] for c in mutations], ['create', 'upload', 'edit'])
        for args in (mutations[0], mutations[2]):
            self.assertIn('--prerelease', args)
            self.assertIn('--latest=false', args)
        self.assertNotIn('--clobber', mutations[1])

    def test_old_run_cannot_publish(self):
        gh = self.run_publish(lambda *args: 'b' * 40)
        self.assertEqual(gh.call_count, 1)

    def test_main_advancing_during_upload_keeps_pointer(self):
        heads = iter([SOURCE, 'b' * 40])
        def api(*args):
            if args[0] == 'api':
                return '[[]]' if '--paginate' in args else next(heads)
            return ''
        gh = self.run_publish(api)
        self.assertFalse(any(c.args[:2] == ('release', 'edit') for c in gh.call_args_list))

    def test_upload_failure_keeps_pointer(self):
        def api(*args):
            if args[0] == 'api':
                return '[[]]' if '--paginate' in args else SOURCE
            if args[:2] == ('release', 'upload'):
                raise RuntimeError('Upload failed')
            if args[:2] == ('release', 'edit'):
                self.fail('Must not change pointer after upload failure')
            return ''
        with self.assertRaises(RuntimeError):
            self.run_publish(api)

    def test_feature_branch_cannot_publish(self):
        self.env['GITHUB_REF'] = 'refs/heads/feature'
        with self.assertRaises(ValueError):
            self.run_publish(lambda *args: self.fail('Must not call GitHub'))


if __name__ == '__main__':
    unittest.main()
