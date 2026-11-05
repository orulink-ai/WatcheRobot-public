import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('package_skill', Path(__file__).resolve().parents[1] / 'package_skill.py')
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class SkillPackagingTests(unittest.TestCase):
    def test_real_bundle_matches_sources_and_is_reproducible(self):
        source = Path(__file__).resolve().parents[1] / 'downloads/watche-robot-app'
        with tempfile.TemporaryDirectory() as tmp:
            first, second = Path(tmp) / 'one.zip', Path(tmp) / 'two.zip'
            module.package(source, first)
            module.package(source, second)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            with zipfile.ZipFile(first) as archive:
                expected = {str(Path(source.name) / path.relative_to(source)): path for path in source.rglob('*') if path.is_file()}
                self.assertEqual(set(archive.namelist()), set(expected))
                for name, path in expected.items():
                    self.assertEqual(archive.read(name), path.read_bytes(), name)

    def test_rejects_symbolic_links(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'skill'
            source.mkdir()
            (source / 'unsafe').symlink_to('/etc/passwd')
            with self.assertRaises(ValueError):
                module.package(source, Path(tmp) / 'bad.zip')


if __name__ == '__main__':
    unittest.main()
