#!/usr/bin/env python3
"""Install a versioned official resource archive through an SD-card reader."""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any

try:
    from .sd_package import InstallError, Package, VERSION_PATTERN, acquire_package
except ImportError:
    from sd_package import InstallError, Package, VERSION_PATTERN, acquire_package


ALLOWED_ROOTS = {"assets"}
ALLOWED_ROOT_FILES = {
    "official_catalog.json",
    "fixed_states.json",
    "resource_manifest.json",
}
LEGACY_ROOT_ENTRIES = {
    "anim",
    "actions",
    "sfx",
    "behavior",
    "resource_catalog.json",
    "resource_manifest.json",
}
MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_EXTRACTED_BYTES = 96 * 1024 * 1024
MAX_SINGLE_FILE_BYTES = 16 * 1024 * 1024
MAX_FILES = 512
SPACE_RESERVE_BYTES = 4 * 1024 * 1024
SUPPORTED_FILESYSTEMS = {"FAT32"}
READER_TRANSACTION_NAME = "reader_transaction.json"
FIXED_STATES = {
    "boot",
    "standby",
    "listening",
    "thinking",
    "speaking",
    "processing",
    "error",
    "upgrade",
}
ASSET_LAYOUT = {
    "animation": ("anim", "anim", ".animpack", "animpack-v2"),
    "action": ("action", "actions", ".json", "firmware-action-json-v1"),
    "sound": ("sfx", "sfx", ".pcm", "pcm-s16le-24khz-mono"),
}


@dataclass(frozen=True)
class DriveInfo:
    root: Path
    label: str
    filesystem: str
    free_bytes: int
    total_bytes: int
    kind: str
    allocation_unit_bytes: int = 4096


@dataclass(frozen=True)
class ArchivePlan:
    version: str
    file_count: int
    extracted_bytes: int
    manifest: dict[str, Any]
    manifest_sha256: str


@dataclass(frozen=True)
class InstallResult:
    version: str
    file_count: int
    extracted_bytes: int
    skipped: bool = False


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="tools/flash sd",
        description="Install an official resource package through an SD-card reader.",
    )
    parser.add_argument("version", nargs="?", help="Optional exact version in vX.Y.Z form; latest is the default.")
    parser.add_argument("--drive", type=Path, help=r"Explicit SD-card root, for example E:\.")
    parser.add_argument("--file", type=Path, dest="local_file", help="Explicit local tar.gz package for offline use.")
    parser.add_argument(
        "--force",
        action="store_true",
        help="Discard an unfinished device-side transaction before installing through the reader.",
    )
    arguments = parser.parse_args(argv)
    if arguments.version and not VERSION_PATTERN.fullmatch(arguments.version):
        parser.error("version must use the vMAJOR.MINOR.PATCH format")
    return arguments


def _windows_volume(root: str) -> tuple[str, str]:
    volume_name = ctypes.create_unicode_buffer(261)
    filesystem_name = ctypes.create_unicode_buffer(261)
    ok = ctypes.windll.kernel32.GetVolumeInformationW(
        ctypes.c_wchar_p(root),
        volume_name,
        len(volume_name),
        None,
        None,
        None,
        filesystem_name,
        len(filesystem_name),
    )
    if not ok:
        return "", ""
    return volume_name.value, filesystem_name.value.upper()


def _normalize_filesystem(value: str) -> str:
    compact = value.upper().replace("-", "").replace("_", "").replace(" ", "")
    return "FAT32" if compact in {"FAT32", "VFAT", "MSDOSFAT32", "MSDOS"} else value.upper()


def _posix_volume(root: Path) -> tuple[str, str, Path]:
    if sys.platform == "darwin":
        result = subprocess.run(
            ["diskutil", "info", "-plist", str(root)],
            check=True,
            capture_output=True,
        )
        info = plistlib.loads(result.stdout)
        mount = Path(str(info.get("MountPoint", ""))).resolve()
        label = str(info.get("VolumeName", ""))
        filesystem = str(info.get("FileSystemPersonality") or info.get("FilesystemType") or "")
        return label, _normalize_filesystem(filesystem), mount
    target = subprocess.run(
        ["findmnt", "-no", "TARGET", "-T", str(root)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    filesystem = subprocess.run(
        ["findmnt", "-no", "FSTYPE", "-T", str(root)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    label = subprocess.run(
        ["findmnt", "-no", "LABEL", "-T", str(root)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    return label, _normalize_filesystem(filesystem), Path(target).resolve()


def _drive_info(root: Path, kind: str) -> DriveInfo:
    usage = shutil.disk_usage(root)
    label = ""
    filesystem = ""
    allocation_unit_bytes = 4096
    if os.name == "nt":
        label, filesystem = _windows_volume(str(root))
        sectors_per_cluster = ctypes.c_ulong()
        bytes_per_sector = ctypes.c_ulong()
        free_clusters = ctypes.c_ulong()
        total_clusters = ctypes.c_ulong()
        if ctypes.windll.kernel32.GetDiskFreeSpaceW(
            ctypes.c_wchar_p(str(root)),
            ctypes.byref(sectors_per_cluster),
            ctypes.byref(bytes_per_sector),
            ctypes.byref(free_clusters),
            ctypes.byref(total_clusters),
        ):
            allocation_unit_bytes = max(1, sectors_per_cluster.value * bytes_per_sector.value)
    else:
        label, filesystem, _mount = _posix_volume(root)
        allocation_unit_bytes = max(1, os.statvfs(root).f_frsize)
    return DriveInfo(root, label, filesystem, usage.free, usage.total, kind, allocation_unit_bytes)


def detect_removable_drives() -> list[DriveInfo]:
    if os.name != "nt":
        return []
    mask = ctypes.windll.kernel32.GetLogicalDrives()
    drives: list[DriveInfo] = []
    for index in range(26):
        if not mask & (1 << index):
            continue
        root_text = f"{chr(ord('A') + index)}:\\"
        if ctypes.windll.kernel32.GetDriveTypeW(root_text) != 2:
            continue
        root = Path(root_text)
        try:
            drives.append(_drive_info(root, "removable"))
        except OSError:
            continue
    return drives


def _normalize_explicit_drive(value: Path) -> Path:
    if os.name != "nt":
        return value.resolve()
    text = str(value).strip().replace("/", "\\")
    if not re.fullmatch(r"[A-Za-z]:\\?", text):
        raise InstallError(r"--drive must be a drive root such as E:\.")
    return Path(text[:2].upper() + "\\")


def inspect_explicit_drive(value: Path) -> DriveInfo:
    root = _normalize_explicit_drive(value)
    if not root.is_dir():
        raise InstallError(f"The selected SD-card drive does not exist: {root}")
    if os.name == "nt":
        system_drive = os.environ.get("SystemDrive", "C:").upper()
        if str(root)[:2].upper() == system_drive:
            raise InstallError(f"Refusing to use the Windows system drive as an SD card: {root}")
        drive_type = ctypes.windll.kernel32.GetDriveTypeW(str(root))
        if drive_type not in (2, 3):
            raise InstallError(f"The selected target is not a writable SD-card drive: {root}")
        kind = "removable" if drive_type == 2 else "explicit-fixed"
    else:
        try:
            _label, _filesystem, mount = _posix_volume(root)
        except (OSError, subprocess.CalledProcessError, plistlib.InvalidFileException) as exc:
            raise InstallError(f"Unable to inspect the selected SD-card mount: {root} ({exc})") from exc
        if root == Path(root.anchor) or mount != root:
            raise InstallError(f"--drive must be the SD-card mount root, not a directory inside another volume: {root}")
        kind = "explicit"
    return _drive_info(root, kind)


def _drive_summary(drive: DriveInfo) -> str:
    label = drive.label or "(no label)"
    return (
        f"{drive.root}  {label}  {drive.filesystem or 'unknown filesystem'}  "
        f"{drive.free_bytes / (1024 * 1024):.1f} MB free"
    )


def select_drive(explicit: Path | None, detected: list[DriveInfo]) -> DriveInfo:
    if explicit is not None:
        return inspect_explicit_drive(explicit)
    if not detected:
        raise InstallError(
            "No writable removable SD card was detected. Insert the SD card or specify it explicitly, "
            r"for example: tools/flash sd --drive E:\."
        )
    if len(detected) > 1:
        details = "\n  ".join(_drive_summary(drive) for drive in detected)
        raise InstallError(
            "Multiple writable removable drives were detected:\n  "
            + details
            + "\nSpecify the target explicitly, for example: "
            + r"tools/flash sd --drive E:\."
        )
    return detected[0]


def validate_drive(drive: DriveInfo) -> DriveInfo:
    if not drive.root.is_dir():
        raise InstallError(f"The selected SD-card drive is unavailable: {drive.root}")
    refreshed = _drive_info(drive.root, drive.kind)
    filesystem = refreshed.filesystem or drive.filesystem
    label = refreshed.label or drive.label
    refreshed = DriveInfo(
        refreshed.root,
        label,
        filesystem,
        refreshed.free_bytes,
        refreshed.total_bytes,
        refreshed.kind,
        refreshed.allocation_unit_bytes,
    )
    if refreshed.filesystem.upper() not in SUPPORTED_FILESYSTEMS:
        actual = refreshed.filesystem or "unknown"
        raise InstallError(
            f"Unsupported SD-card filesystem {actual} on {refreshed.root}. "
            "The current robot firmware requires FAT32."
        )
    probe = refreshed.root / ".watche-sd-write-test.tmp"
    try:
        probe.write_bytes(b"watche")
        probe.unlink()
    except OSError as exc:
        try:
            probe.unlink(missing_ok=True)
        except OSError:
            pass
        raise InstallError(f"The selected SD card is not writable: {refreshed.root} ({exc})") from exc
    return refreshed


def _safe_member_path(name: str) -> PurePosixPath:
    if "\\" in name:
        raise InstallError(f"Archive contains an unsafe path: {name}")
    path = PurePosixPath(name)
    if path.is_absolute() or not path.parts or any(part in ("", ".", "..") for part in path.parts):
        raise InstallError(f"Archive contains an unsafe path: {name}")
    if path.name in ("", ".", ".."):
        raise InstallError(f"Archive contains an unsafe path: {name}")
    first = path.parts[0]
    if not (
        (len(path.parts) == 1 and first in ALLOWED_ROOT_FILES)
        or (len(path.parts) >= 2 and first in ALLOWED_ROOTS)
    ):
        raise InstallError(f"Archive contains an unsupported resource path: {name}")
    return path


def _sha256_stream(handle: Any) -> str:
    digest = hashlib.sha256()
    for chunk in iter(lambda: handle.read(1024 * 1024), b""):
        digest.update(chunk)
    return digest.hexdigest()


def _calculate_bundle_hash(entries: dict[str, str]) -> str:
    digest = hashlib.sha256()
    for relative, sha256 in sorted(entries.items()):
        digest.update(relative.encode())
        digest.update(b"\0")
        digest.update(sha256.encode())
        digest.update(b"\0")
    return digest.hexdigest()


def _read_archive_json(
    archive: tarfile.TarFile,
    member: tarfile.TarInfo,
    label: str,
) -> dict[str, Any]:
    handle = archive.extractfile(member)
    if handle is None:
        raise InstallError(f"Archive {label} is unreadable.")
    try:
        document = json.loads(handle.read().decode("utf-8-sig"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise InstallError(f"Archive {label} is invalid: {exc}") from exc
    if not isinstance(document, dict):
        raise InstallError(f"Archive {label} must be a JSON object.")
    return document


def _validate_catalog_contract(
    catalog: dict[str, Any],
    fixed_states: dict[str, Any],
    expected_files: dict[str, dict[str, Any]],
) -> None:
    expressions = catalog.get("expressions")
    if (
        catalog.get("schema_version") != 2
        or catalog.get("format") != "watche-official-catalog"
        or not isinstance(expressions, list)
        or not expressions
    ):
        raise InstallError("Archive official_catalog.json does not use the supported v2 format.")
    resource_ids: set[str] = set()
    referenced_assets: set[str] = set()
    for order, expression in enumerate(expressions):
        if not isinstance(expression, dict):
            raise InstallError("Archive official catalog contains an invalid expression.")
        resource_id = expression.get("id")
        assets = expression.get("assets")
        if (
            not isinstance(resource_id, str)
            or not re.fullmatch(r"[a-z][a-z0-9_]{0,22}", resource_id)
            or resource_id in resource_ids
            or expression.get("order") != order
            or not isinstance(expression.get("display_name"), str)
            or not isinstance(expression.get("source_record_id"), str)
            or not isinstance(assets, dict)
            or "animation" not in assets
        ):
            raise InstallError("Archive official catalog contains an invalid or duplicate expression.")
        resource_ids.add(resource_id)
        for asset_name, asset in assets.items():
            if asset_name not in ASSET_LAYOUT or not isinstance(asset, dict):
                raise InstallError(f"Archive catalog has an unsupported asset for {resource_id}: {asset_name}")
            kind, directory, extension, asset_format = ASSET_LAYOUT[asset_name]
            sha256 = asset.get("sha256")
            size = asset.get("size")
            relative = f"assets/{directory}/{sha256}{extension}"
            expected = expected_files.get(relative)
            if (
                asset.get("kind") != kind
                or asset.get("format") != asset_format
                or not isinstance(sha256, str)
                or not re.fullmatch(r"[a-f0-9]{64}", sha256)
                or not isinstance(size, int)
                or size <= 0
                or expected is None
                or expected["sha256"] != sha256
                or expected["size"] != size
            ):
                raise InstallError(f"Archive catalog asset reference is invalid: {resource_id}/{asset_name}")
            referenced_assets.add(relative)
    states = fixed_states.get("states")
    if (
        fixed_states.get("schema_version") != 1
        or not isinstance(states, dict)
        or set(states) != FIXED_STATES
        or any(not isinstance(value, str) or value not in resource_ids for value in states.values())
    ):
        raise InstallError("Archive fixed_states.json is incomplete or references an unknown expression.")
    packaged_assets = {path for path in expected_files if path.startswith("assets/")}
    if referenced_assets != packaged_assets:
        raise InstallError("Archive contains an unreferenced asset object or a missing catalog reference.")


def inspect_archive(package: Package) -> ArchivePlan:
    if package.path.stat().st_size > MAX_ARCHIVE_BYTES:
        raise InstallError(f"Archive exceeds the {MAX_ARCHIVE_BYTES}-byte transfer limit.")
    try:
        with tarfile.open(package.path, "r:gz") as archive:
            members: dict[str, tarfile.TarInfo] = {}
            extracted_bytes = 0
            for member in archive.getmembers():
                path = _safe_member_path(member.name)
                if member.isdir():
                    continue
                if not member.isfile():
                    raise InstallError(f"Archive contains an unsupported entry type: {member.name}")
                relative = path.as_posix()
                if relative in members:
                    raise InstallError(f"Archive contains a duplicate path: {relative}")
                if member.size <= 0 or member.size > MAX_SINGLE_FILE_BYTES:
                    raise InstallError(f"Archive file size is invalid: {relative} ({member.size} bytes)")
                members[relative] = member
                extracted_bytes += member.size
            if len(members) > MAX_FILES:
                raise InstallError(f"Archive contains {len(members)} files; limit is {MAX_FILES}.")
            if extracted_bytes > MAX_EXTRACTED_BYTES:
                raise InstallError(
                    f"Archive expands to {extracted_bytes} bytes; limit is {MAX_EXTRACTED_BYTES} bytes."
                )
            if (
                "resource_manifest.json" not in members
                or "official_catalog.json" not in members
                or "fixed_states.json" not in members
            ):
                raise InstallError(
                    "Archive is missing resource_manifest.json, official_catalog.json, or fixed_states.json."
                )
            manifest_handle = archive.extractfile(members["resource_manifest.json"])
            if manifest_handle is None:
                raise InstallError("Archive resource_manifest.json is unreadable.")
            manifest_payload = manifest_handle.read()
            try:
                manifest = json.loads(manifest_payload.decode("utf-8-sig"))
            except (UnicodeError, json.JSONDecodeError) as exc:
                raise InstallError(f"Archive resource_manifest.json is invalid: {exc}") from exc
            if (
                not isinstance(manifest, dict)
                or manifest.get("schema_version") != 2
                or manifest.get("product") != "WatcheRobot-S3"
                or manifest.get("bundle_version") != package.version
                or manifest.get("layout_revision") != 2
            ):
                raise InstallError("Archive resource manifest does not match the selected package.")
            manifest_files = manifest.get("files")
            if not isinstance(manifest_files, list):
                raise InstallError("Archive resource manifest has no valid file list.")
            expected: dict[str, dict[str, Any]] = {}
            for item in manifest_files:
                if not isinstance(item, dict):
                    raise InstallError("Archive resource manifest contains an invalid file entry.")
                relative = item.get("path")
                size = item.get("size")
                sha256 = str(item.get("sha256", "")).lower()
                if (
                    not isinstance(relative, str)
                    or not isinstance(size, int)
                    or size <= 0
                    or not re.fullmatch(r"[a-f0-9]{64}", sha256)
                    or relative in expected
                ):
                    raise InstallError("Archive resource manifest contains an invalid file entry.")
                _safe_member_path(relative)
                expected[relative] = {"size": size, "sha256": sha256}
            actual_paths = set(members) - {"resource_manifest.json"}
            if actual_paths != set(expected):
                raise InstallError("Archive file set does not match resource_manifest.json.")
            actual_hashes: dict[str, str] = {}
            for relative, item in expected.items():
                member = members[relative]
                if member.size != item["size"]:
                    raise InstallError(f"Archive file size does not match the manifest: {relative}")
                handle = archive.extractfile(member)
                if handle is None:
                    raise InstallError(f"Archive file is unreadable: {relative}")
                actual_hash = _sha256_stream(handle)
                if actual_hash != item["sha256"]:
                    raise InstallError(f"Archive file SHA-256 does not match the manifest: {relative}")
                actual_hashes[relative] = actual_hash
            expected_bundle_hash = str(manifest.get("bundle_sha256", "")).lower()
            if (
                not re.fullmatch(r"[a-f0-9]{64}", expected_bundle_hash)
                or _calculate_bundle_hash(actual_hashes) != expected_bundle_hash
            ):
                raise InstallError("Archive bundle SHA-256 does not match resource_manifest.json.")
            _validate_catalog_contract(
                _read_archive_json(archive, members["official_catalog.json"], "official_catalog.json"),
                _read_archive_json(archive, members["fixed_states.json"], "fixed_states.json"),
                expected,
            )
            object_count = sum(path.startswith("assets/") for path in members)
            if object_count != package.object_count:
                raise InstallError("Archive object count does not match ota-manifest.json.")
            return ArchivePlan(
                package.version,
                len(members),
                extracted_bytes,
                manifest,
                hashlib.sha256(manifest_payload).hexdigest(),
            )
    except (OSError, tarfile.TarError) as exc:
        raise InstallError(f"Unable to inspect the resource archive: {exc}") from exc


def _remove_path(path: Path) -> None:
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path)
    else:
        path.unlink(missing_ok=True)


def _write_json_atomic(path: Path, document: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def _recover_reader_transaction(watche_root: Path) -> None:
    transaction = watche_root / "system" / READER_TRANSACTION_NAME
    if not transaction.exists():
        return
    current = watche_root / "official" / "current"
    backup = watche_root / "official" / "rollback.reader"
    staging = watche_root / "staging" / "reader-content"
    print("Recovering an interrupted SD-card reader installation...")
    if backup.exists():
        _remove_path(current)
        backup.replace(current)
    _remove_path(staging)
    transaction.unlink(missing_ok=True)


def _handle_device_transaction(watche_root: Path, force: bool) -> None:
    transaction = watche_root / "system" / "transaction.json"
    if not transaction.exists():
        return
    if not force:
        raise InstallError(
            "The SD card contains an unfinished device resource transaction. "
            "Reinsert it into the robot and let the firmware recover, or rerun this reader install with --force."
        )
    print("Discarding an unfinished device resource transaction because --force was specified.")
    for name in ("transaction.json", "transaction.json.tmp", "transaction.invalid.json", "transaction.invalid.tmp.json"):
        (watche_root / "system" / name).unlink(missing_ok=True)
    _remove_path(watche_root / "staging" / "content")
    (watche_root / "staging" / "upload.tar.gz").unlink(missing_ok=True)


def _active_path(watche: Path, current: Path, relative: str) -> Path:
    parts = PurePosixPath(relative).parts
    return watche.joinpath(*parts) if parts[0] == "assets" else current.joinpath(*parts)


def _extract_archive(package: Package, watche: Path, current_staging: Path, plan: ArchivePlan) -> None:
    _remove_path(current_staging)
    current_staging.mkdir(parents=True)
    with tarfile.open(package.path, "r:gz") as archive:
        files = [member for member in archive.getmembers() if member.isfile()]
        written = 0
        for index, member in enumerate(files, start=1):
            relative = _safe_member_path(member.name)
            target = (
                watche.joinpath(*relative.parts)
                if relative.parts[0] == "assets"
                else current_staging.joinpath(*relative.parts)
            )
            target.parent.mkdir(parents=True, exist_ok=True)
            source = archive.extractfile(member)
            if source is None:
                raise InstallError(f"Archive file is unreadable: {member.name}")
            expected = (
                next(
                    (
                        item
                        for item in plan.manifest["files"]
                        if isinstance(item, dict) and item.get("path") == relative.as_posix()
                    ),
                    None,
                )
                if relative.name != "resource_manifest.json"
                else None
            )
            if expected is not None and target.is_file():
                with target.open("rb") as existing:
                    existing_sha = _sha256_stream(existing)
                if target.stat().st_size == expected["size"] and existing_sha == expected["sha256"]:
                    written += member.size
                    print(
                        f"  [{index}/{len(files)}] {relative.as_posix()} "
                        f"already present ({written * 100 // plan.extracted_bytes}%)"
                    )
                    continue
            temporary = target.with_name(target.name + ".part")
            temporary.unlink(missing_ok=True)
            with temporary.open("wb") as output:
                shutil.copyfileobj(source, output, 1024 * 1024)
            if expected is not None:
                with temporary.open("rb") as extracted:
                    actual_sha = _sha256_stream(extracted)
                if temporary.stat().st_size != expected["size"] or actual_sha != expected["sha256"]:
                    temporary.unlink(missing_ok=True)
                    raise InstallError(f"Extracted resource verification failed before activation: {relative}")
            temporary.replace(target)
            written += member.size
            print(
                f"  [{index}/{len(files)}] {relative.as_posix()} "
                f"({written * 100 // plan.extracted_bytes}%)"
            )


def _verify_active(watche: Path, current: Path, plan: ArchivePlan) -> None:
    manifest_files = {
        item["path"]: item
        for item in plan.manifest["files"]
        if isinstance(item, dict) and isinstance(item.get("path"), str)
    }
    hashes: dict[str, str] = {}
    for relative, item in manifest_files.items():
        path = _active_path(watche, current, relative)
        if not path.is_file():
            raise InstallError(f"Installed resource is missing: {relative}")
        if path.stat().st_size != item["size"]:
            raise InstallError(f"Extracted file size mismatch: {relative}")
        with path.open("rb") as handle:
            actual_hash = _sha256_stream(handle)
        if actual_hash != item["sha256"]:
            raise InstallError(f"Extracted file SHA-256 mismatch: {relative}")
        hashes[relative] = actual_hash
    if _calculate_bundle_hash(hashes) != plan.manifest["bundle_sha256"]:
        raise InstallError("Extracted bundle SHA-256 mismatch.")


def _collect_work_asset_hashes(works: Path) -> set[str]:
    hashes: set[str] = set()
    if not works.is_dir():
        return hashes
    for manifest_path in works.glob("*/work.json"):
        try:
            document = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
        except (OSError, UnicodeError, json.JSONDecodeError):
            continue
        stack: list[Any] = [document]
        while stack:
            value = stack.pop()
            if isinstance(value, dict):
                sha256 = value.get("sha256")
                if isinstance(sha256, str) and re.fullmatch(r"[a-f0-9]{64}", sha256):
                    hashes.add(sha256)
                stack.extend(value.values())
            elif isinstance(value, list):
                stack.extend(value)
    return hashes


def _incoming_asset_paths(plan: ArchivePlan) -> set[str]:
    return {
        item["path"]
        for item in plan.manifest["files"]
        if isinstance(item, dict)
        and isinstance(item.get("path"), str)
        and item["path"].startswith("assets/")
    }


def _cleanup_unreferenced_assets(watche: Path, plan: ArchivePlan) -> None:
    keep_paths = _incoming_asset_paths(plan)
    keep_hashes = _collect_work_asset_hashes(watche / "works")
    assets = watche / "assets"
    if not assets.is_dir():
        return
    for path in assets.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(watche).as_posix()
        if relative in keep_paths or path.stem in keep_hashes:
            continue
        path.unlink()
        print(f"Removed unreferenced asset object: {relative}")


def _installed_version(current: Path) -> str | None:
    manifest_path = current / "resource_manifest.json"
    try:
        document = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError, UnicodeError):
        return None
    version = document.get("bundle_version") if isinstance(document, dict) else None
    return version if isinstance(version, str) and VERSION_PATTERN.fullmatch(version) else None


def _installed_layout_revision(current: Path) -> int | None:
    manifest_path = current / "resource_manifest.json"
    try:
        document = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError, UnicodeError):
        return None
    revision = document.get("layout_revision") if isinstance(document, dict) else None
    return revision if isinstance(revision, int) else None


def required_free_bytes(
    plan: ArchivePlan,
    watche: Path,
    allocation_unit_bytes: int,
) -> int:
    missing_bytes = 0
    missing_files = 0
    for item in plan.manifest["files"]:
        relative = item["path"]
        current = watche / "official" / "current"
        target = _active_path(watche, current, relative)
        valid = False
        if target.is_file() and target.stat().st_size == item["size"]:
            with target.open("rb") as handle:
                valid = _sha256_stream(handle) == item["sha256"]
        if not valid:
            missing_bytes += item["size"]
            missing_files += 1
    allocation_overhead = (missing_files + 4) * max(1, allocation_unit_bytes)
    return missing_bytes + allocation_overhead + SPACE_RESERVE_BYTES


def _cleanup_legacy_entries(card_root: Path) -> None:
    for name in sorted(LEGACY_ROOT_ENTRIES):
        legacy = card_root / name
        if legacy.exists():
            _remove_path(legacy)
            print(f"Removed unsupported legacy resource path: {name}")


def install_package_to_card(package: Package, drive: DriveInfo, force: bool) -> InstallResult:
    drive = validate_drive(drive)
    print("[4/8] Inspecting and validating the resource archive...")
    plan = inspect_archive(package)
    watche = drive.root / "watche"
    system = watche / "system"
    current = watche / "official" / "current"
    backup = watche / "official" / "rollback.reader"
    staging = watche / "staging" / "reader-content"
    transaction = system / READER_TRANSACTION_NAME

    _handle_device_transaction(watche, force)
    system.mkdir(parents=True, exist_ok=True)
    (watche / "official").mkdir(parents=True, exist_ok=True)
    (watche / "staging").mkdir(parents=True, exist_ok=True)
    (watche / "works").mkdir(parents=True, exist_ok=True)
    works_catalog = watche / "works" / "works_catalog.json"
    if not works_catalog.exists():
        _write_json_atomic(works_catalog, {"works": []})
    _write_json_atomic(
        system / "layout.json",
        {"layout_id": "watche-resource-layout", "layout_revision": 2},
    )
    _recover_reader_transaction(watche)

    print("Cleaning the previous official resource view while preserving creator works...")
    _cleanup_legacy_entries(drive.root)
    if current.exists() and _installed_layout_revision(current) != 2:
        _remove_path(current)
        print("Removed the unsupported legacy official resource view.")
    for runtime_name in ("runtime", "runtime.next", "runtime.rollback"):
        _remove_path(watche / runtime_name)
    _cleanup_unreferenced_assets(watche, plan)
    _remove_path(backup)
    _remove_path(staging)

    refreshed_free = shutil.disk_usage(drive.root).free
    required = required_free_bytes(plan, watche, drive.allocation_unit_bytes)
    if refreshed_free < required:
        raise InstallError(
            f"SD card has {refreshed_free} free bytes; at least {required} bytes are required "
            f"for {plan.extracted_bytes} bytes of resources, filesystem allocation, and a "
            f"{SPACE_RESERVE_BYTES}-byte reader safety reserve."
        )

    _write_json_atomic(
        transaction,
        {
            "schema_version": 1,
            "phase": "extracting",
            "version": package.version,
            "started_at": datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z"),
        },
    )
    print(
        f"[5/8] Extracting {plan.file_count} file(s), "
        f"{plan.extracted_bytes / (1024 * 1024):.2f} MB..."
    )
    _extract_archive(package, watche, staging, plan)
    print("[6/8] Verifying extracted files...")
    _verify_active(watche, staging, plan)

    _write_json_atomic(
        transaction,
        {
            "schema_version": 1,
            "phase": "switching",
            "version": package.version,
        },
    )
    print("[7/8] Replacing /watche/official/current...")
    _remove_path(backup)
    had_current = current.exists()
    if had_current:
        current.replace(backup)
    try:
        staging.replace(current)
        _write_json_atomic(
            system / "accepted_official.json",
            {
                "schema_version": 2,
                "version": package.version,
                "bundle_sha256": plan.manifest["bundle_sha256"],
                "manifest_sha256": plan.manifest_sha256,
            },
        )
        _verify_active(watche, current, plan)
        _cleanup_unreferenced_assets(watche, plan)
    except Exception:
        _remove_path(current)
        if had_current and backup.exists():
            backup.replace(current)
        raise

    _remove_path(backup)
    transaction.unlink(missing_ok=True)
    print(
        f"[8/8] Installed {package.version} successfully from {package.source}: "
        f"{plan.file_count} files, {plan.extracted_bytes / (1024 * 1024):.2f} MB."
    )
    print("Existing creator works were preserved.")
    print("The robot will rebuild its in-memory asset index after the SD card is inserted and the device starts.")
    return InstallResult(package.version, plan.file_count, plan.extracted_bytes)


def main(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(sys.argv[1:] if argv is None else argv)
    print("Mode   : SD-card reader")
    print(f"Version: {arguments.version or 'latest'}")
    try:
        print("[1/8] Detecting and validating the SD card...")
        drive = select_drive(arguments.drive, detect_removable_drives())
        drive = validate_drive(drive)
        print(f"Target : {_drive_summary(drive)}")
        with tempfile.TemporaryDirectory(prefix="watche-sd-card-install-") as temporary:
            print("[2/8] Resolving and downloading the official resource package...")
            package = acquire_package(arguments.version, arguments.local_file, Path(temporary))
            print(
                f"[3/8] Package: {package.version}, {package.size} bytes, "
                f"SHA-256 {package.sha256}, source={package.source}"
            )
            install_package_to_card(package, drive, arguments.force)
        return 0
    except (InstallError, OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
