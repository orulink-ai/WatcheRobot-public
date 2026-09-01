from __future__ import annotations

import re
import shlex
from pathlib import Path
from zipfile import ZipFile

from .models import FlashSegment, ParsedFlashPackage

REQUIRED_BINARIES = {"bootloader.bin", "partition-table.bin"}
VERSION_IN_NAME = re.compile(r"(v\d+\.\d+(?:\.\d+)?)", re.IGNORECASE)
APP_BINARY_NAME = "watcherobot-s3.bin"


class PackageParseError(ValueError):
    """Raised when a release zip cannot be parsed into flash segments."""


def infer_version_from_path(package_path: Path) -> str | None:
    candidates = [package_path.name, package_path.parent.name]
    for candidate in candidates:
        match = VERSION_IN_NAME.search(candidate)
        if match:
            return match.group(1)
    return None


def _normalize_zip_name(name: str) -> str:
    return name.replace("\\", "/").lstrip("./")


def _find_zip_member(zf: ZipFile, requested_path: str) -> str:
    normalized = _normalize_zip_name(requested_path)
    names = zf.namelist()
    normalized_map = {_normalize_zip_name(name): name for name in names}

    if normalized in normalized_map:
        return normalized_map[normalized]

    basename = Path(normalized).name.lower()
    basename_matches = [
        original_name
        for original_name in names
        if Path(_normalize_zip_name(original_name)).name.lower() == basename
    ]
    if len(basename_matches) == 1:
        return basename_matches[0]
    if len(basename_matches) > 1:
        raise PackageParseError(f"ZIP 内存在多个同名文件，无法唯一匹配: {requested_path}")

    raise PackageParseError(f"ZIP 内缺少烧录文件: {requested_path}")


def _load_flash_args(zf: ZipFile) -> str:
    names = zf.namelist()
    exact = [name for name in names if _normalize_zip_name(name).lower() == "flash_args.txt"]
    if exact:
        return zf.read(exact[0]).decode("utf-8")

    basename_matches = [
        name for name in names if Path(_normalize_zip_name(name)).name.lower() == "flash_args.txt"
    ]
    if len(basename_matches) == 1:
        return zf.read(basename_matches[0]).decode("utf-8")
    if len(basename_matches) > 1:
        raise PackageParseError("ZIP 内存在多个 flash_args.txt，无法确定使用哪个。")

    raise PackageParseError("ZIP 内缺少 flash_args.txt。")


def parse_flash_package(package_path: Path) -> ParsedFlashPackage:
    if not package_path.exists():
        raise PackageParseError(f"ZIP 不存在: {package_path}")

    with ZipFile(package_path) as zf:
        flash_args_content = _load_flash_args(zf)
        lines = [line.strip() for line in flash_args_content.splitlines() if line.strip()]
        if len(lines) < 2:
            raise PackageParseError("flash_args.txt 内容不完整。")

        flag_tokens = shlex.split(lines[0])
        flash_mode = "dio"
        flash_freq = "80m"
        flash_size = "16MB"

        for index, token in enumerate(flag_tokens):
            if token == "--flash_mode" and index + 1 < len(flag_tokens):
                flash_mode = flag_tokens[index + 1]
            elif token == "--flash_freq" and index + 1 < len(flag_tokens):
                flash_freq = flag_tokens[index + 1]
            elif token == "--flash_size" and index + 1 < len(flag_tokens):
                flash_size = flag_tokens[index + 1]

        segments: list[FlashSegment] = []
        basenames: set[str] = set()
        for line in lines[1:]:
            tokens = shlex.split(line)
            if len(tokens) != 2:
                raise PackageParseError(f"无法解析 flash_args 行: {line}")

            try:
                offset = int(tokens[0], 16)
            except ValueError as exc:
                raise PackageParseError(f"非法烧录地址: {tokens[0]}") from exc

            member_name = _find_zip_member(zf, tokens[1])
            file_name = Path(_normalize_zip_name(member_name)).name
            data = zf.read(member_name)
            segments.append(
                FlashSegment(
                    offset=offset,
                    path=tokens[1],
                    file_name=file_name,
                    data=data,
                )
            )
            basenames.add(file_name)

        missing = sorted(REQUIRED_BINARIES - basenames)
        if missing:
            raise PackageParseError(f"ZIP 缺少必需二进制文件: {', '.join(missing)}")

        if not any(segment.file_name.lower() == APP_BINARY_NAME for segment in segments):
            raise PackageParseError("ZIP 缺少主应用固件段（期望 WatcheRobot-S3.bin）。")

        if not segments:
            raise PackageParseError("flash_args.txt 未定义任何烧录 segment。")

        return ParsedFlashPackage(
            package_path=package_path,
            version=infer_version_from_path(package_path),
            chip="esp32s3",
            flash_mode=flash_mode,
            flash_freq=flash_freq,
            flash_size=flash_size,
            segments=segments,
        )
