from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ReleaseEntry:
    version: str
    zip_path: Path
    zip_name: str
    release_dir: Path
    notes_path: Path | None = None


@dataclass(frozen=True)
class FlashSegment:
    offset: int
    path: str
    file_name: str
    data: bytes


@dataclass(frozen=True)
class ParsedFlashPackage:
    package_path: Path
    version: str | None
    chip: str
    flash_mode: str
    flash_freq: str
    flash_size: str
    segments: list[FlashSegment]
