#!/usr/bin/env python3
"""Validate UE5 source-plugin release archives and their checksums."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any


PLUGIN_NAME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.]+)?$")
SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")
LFS_POINTER_PREFIX = b"version https://git-lfs.github.com/spec/v1"
FORBIDDEN_DIRECTORIES = {"Binaries", "DerivedDataCache", "Intermediate"}


class ValidationError(RuntimeError):
    """Raised when a release artifact violates the publish contract."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--dist", required=True, type=Path)
    parser.add_argument("--version", required=True)
    return parser.parse_args()


def read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ValidationError(f"cannot read {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise ValidationError(f"invalid JSON in {path}: {exc}") from exc


def load_plugins(manifest_path: Path) -> list[str]:
    data = read_json(manifest_path)
    if not isinstance(data, dict) or data.get("schema_version") != 1:
        raise ValidationError("manifest schema_version must be 1")

    plugins = data.get("plugins")
    if not isinstance(plugins, list) or not plugins:
        raise ValidationError("manifest plugins must be a non-empty list")
    if any(not isinstance(item, str) or not PLUGIN_NAME_RE.fullmatch(item) for item in plugins):
        raise ValidationError("manifest contains an invalid plugin name")
    if len(set(plugins)) != len(plugins):
        raise ValidationError("manifest contains duplicate plugin names")
    if "TapContractTesting" in plugins:
        raise ValidationError("TapContractTesting must not be included in the publish manifest")
    return plugins


def validate_entry_name(archive_name: str, entry_name: str) -> PurePosixPath:
    if "\\" in entry_name:
        raise ValidationError(f"{archive_name}: zip entry uses a backslash: {entry_name}")
    path = PurePosixPath(entry_name)
    if path.is_absolute() or ".." in path.parts:
        raise ValidationError(f"{archive_name}: unsafe zip entry: {entry_name}")
    return path


def validate_descriptor(archive_name: str, descriptor_name: str, raw: bytes, version: str) -> None:
    try:
        data = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValidationError(
            f"{archive_name}: invalid descriptor {descriptor_name}: {exc}"
        ) from exc

    if not isinstance(data, dict):
        raise ValidationError(f"{archive_name}: descriptor must contain a JSON object")
    if data.get("VersionName") != version:
        raise ValidationError(
            f"{archive_name}: VersionName is {data.get('VersionName')!r}, expected {version!r}"
        )
    if "Installed" in data and data["Installed"] is not False:
        raise ValidationError(
            f"{archive_name}: Installed must be false or omitted for a source plugin"
        )
    if data.get("EngineVersion") not in (None, ""):
        raise ValidationError(
            f"{archive_name}: EngineVersion must be omitted for the shared "
            "cross-version source package"
        )


def validate_archive(archive_path: Path, plugin: str, version: str) -> None:
    descriptor_name = f"{plugin}/{plugin}.uplugin"
    try:
        with zipfile.ZipFile(archive_path) as archive:
            names = archive.namelist()
            duplicates = sorted(name for name, count in Counter(names).items() if count > 1)
            if duplicates:
                raise ValidationError(
                    f"{archive_path.name}: duplicate zip entries: {', '.join(duplicates)}"
                )

            roots: set[str] = set()
            for info in archive.infolist():
                entry_path = validate_entry_name(archive_path.name, info.filename)
                if not entry_path.parts:
                    continue
                roots.add(entry_path.parts[0])
                forbidden = FORBIDDEN_DIRECTORIES.intersection(entry_path.parts)
                if forbidden:
                    raise ValidationError(
                        f"{archive_path.name}: forbidden directory in {info.filename}: "
                        f"{', '.join(sorted(forbidden))}"
                    )
                if not info.is_dir():
                    with archive.open(info) as entry:
                        if entry.read(len(LFS_POINTER_PREFIX)).startswith(LFS_POINTER_PREFIX):
                            raise ValidationError(
                                f"{archive_path.name}: Git LFS pointer found in {info.filename}"
                            )

            if roots != {plugin}:
                raise ValidationError(
                    f"{archive_path.name}: root directories are {sorted(roots)}, "
                    f"expected [{plugin!r}]"
                )
            if descriptor_name not in names:
                raise ValidationError(f"{archive_path.name}: missing {descriptor_name}")
            validate_descriptor(
                archive_path.name,
                descriptor_name,
                archive.read(descriptor_name),
                version,
            )

            bad_entry = archive.testzip()
            if bad_entry is not None:
                raise ValidationError(f"{archive_path.name}: CRC check failed for {bad_entry}")
    except (OSError, zipfile.BadZipFile) as exc:
        raise ValidationError(f"cannot read {archive_path}: {exc}") from exc


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as exc:
        raise ValidationError(f"cannot hash {path}: {exc}") from exc
    return digest.hexdigest()


def load_checksums(checksums_path: Path) -> dict[str, str]:
    try:
        lines = checksums_path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise ValidationError(f"cannot read {checksums_path}: {exc}") from exc

    checksums: dict[str, str] = {}
    for line_number, line in enumerate(lines, start=1):
        if not line.strip():
            continue
        fields = line.split(maxsplit=1)
        if len(fields) != 2 or not SHA256_RE.fullmatch(fields[0]):
            raise ValidationError(f"checksums.txt:{line_number}: invalid SHA256 entry")
        filename = fields[1]
        if filename.startswith("*"):
            filename = filename[1:]
        if not filename or Path(filename).name != filename or "\\" in filename:
            raise ValidationError(f"checksums.txt:{line_number}: invalid filename {filename!r}")
        if filename in checksums:
            raise ValidationError(f"checksums.txt:{line_number}: duplicate entry for {filename}")
        checksums[filename] = fields[0].lower()
    return checksums


def validate_checksums(dist_dir: Path, expected_archives: set[str]) -> None:
    checksums = load_checksums(dist_dir / "checksums.txt")
    actual_entries = set(checksums)
    if actual_entries != expected_archives:
        missing = ", ".join(sorted(expected_archives - actual_entries)) or "-"
        extra = ", ".join(sorted(actual_entries - expected_archives)) or "-"
        raise ValidationError(f"checksum entries mismatch; missing={missing} extra={extra}")

    for filename in sorted(expected_archives):
        actual_digest = sha256(dist_dir / filename)
        if actual_digest != checksums[filename]:
            raise ValidationError(
                f"checksum mismatch for {filename}: expected {checksums[filename]}, "
                f"got {actual_digest}"
            )


def validate_release(manifest_path: Path, dist_dir: Path, version: str) -> int:
    if not VERSION_RE.fullmatch(version):
        raise ValidationError(f"invalid version {version!r}")
    if not dist_dir.is_dir():
        raise ValidationError(f"dist directory does not exist: {dist_dir}")

    plugins = load_plugins(manifest_path)
    expected_archives = {
        f"TapSDK-UE5-{plugin}-v{version}.zip"
        for plugin in plugins
    }
    actual_archives = {path.name for path in dist_dir.glob("*.zip")}
    if actual_archives != expected_archives:
        missing = ", ".join(sorted(expected_archives - actual_archives)) or "-"
        extra = ", ".join(sorted(actual_archives - expected_archives)) or "-"
        raise ValidationError(f"plugin zip set mismatch; missing={missing} extra={extra}")

    for plugin in plugins:
        validate_archive(
            dist_dir / f"TapSDK-UE5-{plugin}-v{version}.zip",
            plugin,
            version,
        )
    validate_checksums(dist_dir, expected_archives)
    return len(plugins)


def main() -> int:
    args = parse_args()
    try:
        plugin_count = validate_release(args.manifest, args.dist, args.version)
    except ValidationError as exc:
        print(f"release validation failed: {exc}", file=sys.stderr)
        return 1
    print(f"validated {plugin_count} UE5 source-plugin archives and checksums")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
