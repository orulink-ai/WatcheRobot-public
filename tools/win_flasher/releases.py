from __future__ import annotations

import re
from pathlib import Path

from .models import ReleaseEntry

VERSION_PATTERN = re.compile(r"^v(\d+)\.(\d+)(?:\.(\d+))?$", re.IGNORECASE)


def get_repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def get_release_root(repo_root: Path | None = None) -> Path:
    root = repo_root or get_repo_root()
    return root / ".local" / "release-zips"


def parse_version_key(version: str) -> tuple[int, int, int]:
    match = VERSION_PATTERN.fullmatch(version)
    if not match:
        raise ValueError(f"Invalid release version: {version}")
    major, minor, patch = match.groups()
    return (int(major), int(minor), int(patch or 0))


def _score_zip(version: str, zip_path: Path) -> tuple[int, int, str]:
    name = zip_path.name.lower()
    exact_version = int(version.lower() in name)
    chip_hint = int("esp32s3" in name)
    return (exact_version, chip_hint, name)


def _pick_release_zip(version: str, release_dir: Path) -> Path | None:
    zips = [path for path in release_dir.glob("*.zip") if path.is_file()]
    if not zips:
        return None
    return sorted(zips, key=lambda path: _score_zip(version, path), reverse=True)[0]


def scan_release_entries(repo_root: Path | None = None) -> list[ReleaseEntry]:
    release_root = get_release_root(repo_root)
    if not release_root.exists():
        return []

    entries: list[ReleaseEntry] = []
    for release_dir in release_root.iterdir():
        if not release_dir.is_dir():
            continue
        if not VERSION_PATTERN.fullmatch(release_dir.name):
            continue

        zip_path = _pick_release_zip(release_dir.name, release_dir)
        if zip_path is None:
            continue

        notes_path = release_dir / "RELEASE_NOTES.md"
        entries.append(
            ReleaseEntry(
                version=release_dir.name,
                zip_path=zip_path,
                zip_name=zip_path.name,
                release_dir=release_dir,
                notes_path=notes_path if notes_path.exists() else None,
            )
        )

    return sorted(entries, key=lambda entry: parse_version_key(entry.version), reverse=True)


def get_latest_release(repo_root: Path | None = None) -> ReleaseEntry | None:
    entries = scan_release_entries(repo_root)
    return entries[0] if entries else None
