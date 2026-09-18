#!/usr/bin/env python3
"""Resolve and verify official SD resource packages."""

from __future__ import annotations

import hashlib
import json
import re
import tarfile
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any


REPOSITORY = "orulink-ai/WatcheRobot_sd"
GITHUB_BASE = f"https://github.com/{REPOSITORY}/releases"
TOS_BASE = "https://erroright.tos-cn-guangzhou.volces.com/WatcherRobot/sd"
VERSION_PATTERN = re.compile(r"^v\d+\.\d+\.\d+$")
SHA256_PATTERN = re.compile(r"^[a-f0-9]{64}$")
PROTOCOL_PREFIX = "WRSD/2"
HTTP_TIMEOUT_SECONDS = 30


class InstallError(RuntimeError):
    pass


@dataclass(frozen=True)
class Package:
    version: str
    path: Path
    size: int
    sha256: str
    source: str
    expanded_size: int
    file_count: int
    object_count: int


def source_manifest_url(source: str, version: str | None) -> str:
    if source == "github":
        if version is None:
            return f"{GITHUB_BASE}/latest/download/ota-manifest.json"
        return f"{GITHUB_BASE}/download/{version}/ota-manifest.json"
    if source == "tos":
        if version is None:
            return f"{TOS_BASE}/latest.json"
        return f"{TOS_BASE}/{version}/ota-manifest.json"
    raise ValueError(f"Unknown resource source: {source}")


def derived_archive_url(source: str, version: str, name: str) -> str:
    if source == "github":
        return f"{GITHUB_BASE}/download/{version}/{name}"
    if source == "tos":
        return f"{TOS_BASE}/{version}/{name}"
    raise ValueError(f"Unknown resource source: {source}")


def validate_manifest(document: Any, requested_version: str | None) -> tuple[str, dict[str, Any]]:
    if not isinstance(document, dict) or document.get("schema_version") != 3:
        raise InstallError("Unsupported OTA manifest schema; SD layout revision 2 is required.")
    if document.get("product") != "WatcheRobot-S3":
        raise InstallError("OTA manifest targets a different product.")
    if document.get("layout_revision") != 2 or document.get("protocol") != PROTOCOL_PREFIX:
        raise InstallError("OTA manifest does not target SD layout revision 2 / WRSD/2.")
    version = document.get("version")
    if not isinstance(version, str) or not VERSION_PATTERN.fullmatch(version):
        raise InstallError("OTA manifest contains an invalid version.")
    if requested_version is not None and version != requested_version:
        raise InstallError(f"OTA manifest returned {version}; expected {requested_version}.")
    archive = document.get("archive")
    if not isinstance(archive, dict) or archive.get("format") != "tar.gz":
        raise InstallError("OTA manifest does not describe a tar.gz resource archive.")
    name = archive.get("name") or f"watche-sd-resources-{version}.tar.gz"
    size = archive.get("size")
    expanded_size = archive.get("expanded_size")
    file_count = archive.get("file_count")
    object_count = archive.get("object_count")
    sha256 = str(archive.get("sha256", "")).lower()
    if name != f"watche-sd-resources-{version}.tar.gz":
        raise InstallError("OTA archive name does not match its version.")
    if not isinstance(size, int) or size <= 0:
        raise InstallError("OTA manifest contains an invalid archive size.")
    if not isinstance(expanded_size, int) or expanded_size <= 0:
        raise InstallError("OTA manifest contains an invalid expanded archive size.")
    if not isinstance(file_count, int) or not 1 <= file_count <= 512:
        raise InstallError("OTA manifest contains an invalid file count.")
    if not isinstance(object_count, int) or not 1 <= object_count <= file_count:
        raise InstallError("OTA manifest contains an invalid object count.")
    if not SHA256_PATTERN.fullmatch(sha256):
        raise InstallError("OTA manifest contains an invalid archive SHA-256.")
    return version, {**archive, "name": name, "size": size, "sha256": sha256}


def request_bytes(url: str, timeout: int = HTTP_TIMEOUT_SECONDS) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "WatcheRobot-SD-Installer/1"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read()


def download_file(url: str, destination: Path, expected_size: int, expected_sha256: str) -> None:
    request = urllib.request.Request(url, headers={"User-Agent": "WatcheRobot-SD-Installer/1"})
    digest = hashlib.sha256()
    received = 0
    last_percent = -1
    with urllib.request.urlopen(request, timeout=HTTP_TIMEOUT_SECONDS) as response, destination.open("wb") as output:
        while True:
            chunk = response.read(256 * 1024)
            if not chunk:
                break
            output.write(chunk)
            digest.update(chunk)
            received += len(chunk)
            percent = min(100, int(received * 100 / expected_size))
            if percent >= last_percent + 5 or percent == 100:
                print(f"  Downloading... {percent}% ({received}/{expected_size} bytes)")
                last_percent = percent
    if received != expected_size:
        raise InstallError(f"Archive size mismatch: expected {expected_size}, got {received}.")
    actual_sha256 = digest.hexdigest()
    if actual_sha256 != expected_sha256:
        raise InstallError(f"Archive SHA-256 mismatch: expected {expected_sha256}, got {actual_sha256}.")


def acquire_remote_package(source: str, requested_version: str | None, directory: Path) -> Package:
    manifest_url = source_manifest_url(source, requested_version)
    print(f"[Source] Trying {source.upper()}: {manifest_url}")
    try:
        manifest = json.loads(request_bytes(manifest_url).decode("utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError, urllib.error.URLError) as exc:
        raise InstallError(f"{source.upper()} manifest download failed: {exc}") from exc
    version, archive = validate_manifest(manifest, requested_version)
    explicit_url = archive.get(f"{source}_url")
    archive_url = explicit_url if isinstance(explicit_url, str) and explicit_url else derived_archive_url(
        source, version, archive["name"]
    )
    destination = directory / archive["name"]
    try:
        download_file(archive_url, destination, archive["size"], archive["sha256"])
    except (OSError, urllib.error.URLError) as exc:
        raise InstallError(f"{source.upper()} archive download failed: {exc}") from exc
    return Package(
        version,
        destination,
        archive["size"],
        archive["sha256"],
        source,
        archive["expanded_size"],
        archive["file_count"],
        archive["object_count"],
    )


def inspect_local_package(path: Path, requested_version: str | None) -> Package:
    if not path.is_file():
        raise InstallError(f"Local package does not exist: {path}")
    try:
        with tarfile.open(path, "r:gz") as archive:
            member = archive.getmember("resource_manifest.json")
            handle = archive.extractfile(member)
            if handle is None:
                raise InstallError("Local package has no readable resource_manifest.json.")
            manifest = json.load(handle)
            members = [item for item in archive.getmembers() if item.isfile()]
    except (KeyError, OSError, tarfile.TarError, json.JSONDecodeError) as exc:
        raise InstallError(f"Invalid local resource package: {exc}") from exc
    version = manifest.get("bundle_version")
    if not isinstance(version, str) or not VERSION_PATTERN.fullmatch(version):
        raise InstallError("Local package contains an invalid bundle version.")
    if requested_version is not None and requested_version != version:
        raise InstallError(f"Local package is {version}; requested {requested_version}.")
    if manifest.get("schema_version") != 2 or manifest.get("layout_revision") != 2:
        raise InstallError("Local package does not use SD layout revision 2.")
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return Package(
        version,
        path,
        path.stat().st_size,
        digest.hexdigest(),
        "local",
        sum(item.size for item in members),
        len(members),
        sum(item.name.startswith("assets/") for item in members),
    )


def acquire_package(
    requested_version: str | None, local_file: Path | None, directory: Path
) -> Package:
    if local_file is not None:
        print("[Source] Using the explicitly selected local package.")
        return inspect_local_package(local_file, requested_version)
    errors: list[str] = []
    for source in ("github", "tos"):
        source_directory = directory / source
        source_directory.mkdir()
        try:
            return acquire_remote_package(source, requested_version, source_directory)
        except InstallError as exc:
            errors.append(str(exc))
            print(f"  {source.upper()} failed: {exc}")
    raise InstallError("No official resource source succeeded:\n  - " + "\n  - ".join(errors))
