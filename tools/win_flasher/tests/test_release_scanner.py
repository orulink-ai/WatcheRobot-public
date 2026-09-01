from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from tools.win_flasher.releases import get_latest_release, scan_release_entries


class ReleaseScannerTests(unittest.TestCase):
    def _write_release_zip(self, repo_root: Path, version: str) -> None:
        release_dir = repo_root / ".local" / "release-zips" / version
        release_dir.mkdir(parents=True, exist_ok=True)
        (release_dir / f"WatcheRobot-S3-{version}-esp32s3.zip").write_bytes(b"zip")

    def test_scans_and_sorts_releases(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            for version in ["v0.1.8", "v0.1.7", "v0.1.6", "v0.1.5"]:
                self._write_release_zip(repo_root, version)

            entries = scan_release_entries(repo_root)
            versions = [entry.version for entry in entries]
            self.assertEqual(versions, ["v0.1.8", "v0.1.7", "v0.1.6", "v0.1.5"])

    def test_uppercase_v_release_is_supported_and_sorted(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            for version in ["v2.2.1", "V2.3.0", "v0.2.6"]:
                self._write_release_zip(repo_root, version)

            latest = get_latest_release(repo_root)
            self.assertIsNotNone(latest)
            assert latest is not None
            self.assertEqual(latest.version, "V2.3.0")
            self.assertEqual(latest.zip_name, "WatcheRobot-S3-V2.3.0-esp32s3.zip")

    def test_two_part_major_release_is_supported_and_sorted(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            for version in ["V2.4.1", "V3.1", "v0.2.6"]:
                self._write_release_zip(repo_root, version)

            latest = get_latest_release(repo_root)
            self.assertIsNotNone(latest)
            assert latest is not None
            self.assertEqual(latest.version, "V3.1")
            self.assertEqual(latest.zip_name, "WatcheRobot-S3-V3.1-esp32s3.zip")


if __name__ == "__main__":
    unittest.main()
