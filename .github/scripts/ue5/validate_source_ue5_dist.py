#!/usr/bin/env python3
"""Validate that a dist checkout exactly matches the UE5 release archives."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import tempfile
import zipfile
from pathlib import Path

from validate_source_ue5_release import (
    LFS_POINTER_PREFIX,
    ValidationError,
    VERSION_RE,
    load_checksums,
    load_plugins,
    validate_release,
)


SOURCE_REF_RE_LENGTH = 40


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-ref", required=True)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--dist", required=True, type=Path)
    parser.add_argument("--target", required=True, type=Path)
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_release_manifest(path: Path) -> dict[str, object]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValidationError(f"cannot read dist release manifest {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise ValidationError("dist release-manifest.json must contain an object")
    return data


def collect_files(root: Path) -> dict[str, Path]:
    files: dict[str, Path] = {}
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ValidationError(f"symbolic link found in dist source: {path.relative_to(root)}")
        if path.is_file():
            files[path.relative_to(root).as_posix()] = path
    return files


def extract_archives(dist_dir: Path, plugins: list[str], version: str, target: Path) -> None:
    for plugin in plugins:
        archive_path = dist_dir / f"TapSDK-UE5-{plugin}-v{version}.zip"
        with zipfile.ZipFile(archive_path) as archive:
            archive.extractall(target)


def validate_source_tree(expected: Path, actual: Path) -> int:
    expected_files = collect_files(expected)
    actual_files = collect_files(actual)
    if set(expected_files) != set(actual_files):
        missing = sorted(set(expected_files) - set(actual_files))
        extra = sorted(set(actual_files) - set(expected_files))
        raise ValidationError(
            "dist Plugins file set mismatch; "
            f"missing={missing[:20] or '-'} extra={extra[:20] or '-'}"
        )

    for relative_path in sorted(expected_files):
        actual_path = actual_files[relative_path]
        with actual_path.open("rb") as source:
            if source.read(len(LFS_POINTER_PREFIX)) == LFS_POINTER_PREFIX:
                raise ValidationError(f"unresolved Git LFS pointer in dist source: {relative_path}")
        if sha256(expected_files[relative_path]) != sha256(actual_path):
            raise ValidationError(f"dist source content mismatch: {relative_path}")
    return len(actual_files)


def validate_dist(
    version: str,
    source_ref: str,
    manifest_path: Path,
    dist_dir: Path,
    target_dir: Path,
) -> tuple[int, int]:
    if len(source_ref) != SOURCE_REF_RE_LENGTH or any(
        character not in "0123456789abcdef" for character in source_ref
    ):
        raise ValidationError("source_ref must be a full lowercase 40-character commit SHA")

    plugin_count = validate_release(manifest_path, dist_dir, version)
    plugins = load_plugins(manifest_path)
    expected_archives = {
        f"TapSDK-UE5-{plugin}-v{version}.zip"
        for plugin in plugins
    }
    checksums = load_checksums(dist_dir / "checksums.txt")
    if set(checksums) != expected_archives:
        raise ValidationError("local checksums do not match the publish plugin set")

    target_plugins = target_dir / "Plugins"
    if not target_plugins.is_dir():
        raise ValidationError(f"dist Plugins directory does not exist: {target_plugins}")
    target_plugin_names = sorted(path.name for path in target_plugins.iterdir() if path.is_dir())
    if target_plugin_names != sorted(plugins):
        raise ValidationError("dist plugin directory set does not match the publish manifest")

    release_manifest = load_release_manifest(target_dir / "release-manifest.json")
    native = release_manifest.get('native_dependencies')
    if native is not None:
        if (not isinstance(native, dict)
                or not all(isinstance(native.get(key), str) and VERSION_RE.fullmatch(native[key])
                           for key in ('android_version', 'ios_version'))
                or not isinstance(native.get('ios_source_ref'), str)
                or not re.fullmatch(r'[0-9a-f]{40}', native['ios_source_ref'])):
            raise ValidationError('invalid native_dependencies in release manifest')
    expected_manifest_values: dict[str, object] = {
        "version": version,
        "source_repository": "taptap/tapsdk-monorepo",
        "source_ref": source_ref,
        "engine_versions": ["5.7", "5.8"],
        "plugins": plugins,
        "archives": {
            filename: checksums[filename]
            for filename in sorted(checksums)
        },
    }
    for key, expected_value in expected_manifest_values.items():
        if release_manifest.get(key) != expected_value:
            raise ValidationError(
                f"dist release manifest {key} mismatch: "
                f"got {release_manifest.get(key)!r}, expected {expected_value!r}"
            )

    with tempfile.TemporaryDirectory(prefix="ue5-dist-compare-") as temporary_directory:
        expected_plugins = Path(temporary_directory) / "Plugins"
        expected_plugins.mkdir()
        extract_archives(dist_dir, plugins, version, expected_plugins)
        file_count = validate_source_tree(expected_plugins, target_plugins)
    return plugin_count, file_count


def main() -> int:
    args = parse_args()
    try:
        plugin_count, file_count = validate_dist(
            args.version,
            args.source_ref,
            args.manifest,
            args.dist,
            args.target,
        )
    except (OSError, zipfile.BadZipFile, ValidationError) as exc:
        print(f"dist source validation failed: {exc}", file=sys.stderr)
        return 1
    print(f"validated dist source for {plugin_count} plugins and {file_count} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
