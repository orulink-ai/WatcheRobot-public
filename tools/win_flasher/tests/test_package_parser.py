from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile

from tools.win_flasher.package_parser import PackageParseError, parse_flash_package


class PackageParserTests(unittest.TestCase):
    def test_missing_flash_args_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            zip_path = Path(temp_dir) / "bad.zip"
            with ZipFile(zip_path, "w") as zf:
                zf.writestr("bootloader.bin", b"boot")

            with self.assertRaises(PackageParseError):
                parse_flash_package(zip_path)

    def test_basename_fallback_resolves_nested_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            zip_path = Path(temp_dir) / "fallback.zip"
            with ZipFile(zip_path, "w") as zf:
                zf.writestr(
                    "flash_args.txt",
                    "\n".join(
                        [
                            "--flash_mode dio --flash_freq 80m --flash_size 16MB",
                            "0x0 bootloader/bootloader.bin",
                            "0x8000 partition_table/partition-table.bin",
                            "0x10000 app/WatcheRobot-S3.bin",
                        ]
                    ),
                )
                zf.writestr("bootloader.bin", b"boot")
                zf.writestr("partition-table.bin", b"table")
                zf.writestr("WatcheRobot-S3.bin", b"app")

            package = parse_flash_package(zip_path)
            self.assertEqual([segment.file_name for segment in package.segments], [
                "bootloader.bin",
                "partition-table.bin",
                "WatcheRobot-S3.bin",
            ])

    def test_infers_uppercase_v_release_version(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            release_dir = Path(temp_dir) / "V2.3.0"
            release_dir.mkdir()
            zip_path = release_dir / "WatcheRobot-S3-V2.3.0-esp32s3.zip"
            with ZipFile(zip_path, "w") as zf:
                zf.writestr(
                    "WatcheRobot-S3-V2.3.0-esp32s3/flash_args.txt",
                    "\n".join(
                        [
                            "--flash_mode dio --flash_freq 80m --flash_size 16MB",
                            "0x0 bootloader.bin",
                            "0x8000 partition-table.bin",
                            "0x10000 WatcheRobot-S3.bin",
                        ]
                    ),
                )
                zf.writestr("WatcheRobot-S3-V2.3.0-esp32s3/bootloader.bin", b"boot")
                zf.writestr("WatcheRobot-S3-V2.3.0-esp32s3/partition-table.bin", b"table")
                zf.writestr("WatcheRobot-S3-V2.3.0-esp32s3/WatcheRobot-S3.bin", b"app")

            package = parse_flash_package(zip_path)
            self.assertEqual(package.version, "V2.3.0")

    def test_parse_two_part_dual_ota_release_version(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            release_dir = Path(temp_dir) / "V3.1"
            release_dir.mkdir()
            zip_path = release_dir / "WatcheRobot-S3-V3.1-esp32s3.zip"
            with ZipFile(zip_path, "w") as zf:
                zf.writestr(
                    "WatcheRobot-S3-V3.1-esp32s3/flash_args.txt",
                    "\n".join(
                        [
                            "--flash_mode dio --flash_freq 80m --flash_size 16MB",
                            "0x0 bootloader.bin",
                            "0x8000 partition-table.bin",
                            "0xf000 ota_data_initial.bin",
                            "0x20000 WatcheRobot-S3.bin",
                        ]
                    ),
                )
                zf.writestr("WatcheRobot-S3-V3.1-esp32s3/bootloader.bin", b"boot")
                zf.writestr("WatcheRobot-S3-V3.1-esp32s3/partition-table.bin", b"table")
                zf.writestr("WatcheRobot-S3-V3.1-esp32s3/ota_data_initial.bin", b"ota")
                zf.writestr("WatcheRobot-S3-V3.1-esp32s3/WatcheRobot-S3.bin", b"app")

            package = parse_flash_package(zip_path)
            self.assertEqual(package.version, "V3.1")
            self.assertEqual(package.segments[3].offset, 0x20000)
            self.assertEqual(package.segments[3].file_name, "WatcheRobot-S3.bin")


if __name__ == "__main__":
    unittest.main()
