#!/usr/bin/env python3
"""Synchronize a published UE5 tag into main without replacing repository configuration."""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from validate_source_ue5_dist import validate_dist
from validate_source_ue5_release import ValidationError, VERSION_RE


SYNC_FILES = (
    '.github/ue5-release-context.json',
)


def run(*args: str, cwd: Path | None = None) -> str:
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def compare_versions(left: str, right: str) -> int:
    """Semver precedence, including numeric prerelease identifiers (beta.10 > beta.2)."""
    def parse(version: str) -> tuple[tuple[int, ...], list[str]]:
        if not VERSION_RE.fullmatch(version):
            raise ValidationError(f'invalid release version: {version}')
        core, _, suffix = version.partition('-')
        return tuple(map(int, core.split('.'))), suffix.split('.') if suffix else []

    left_core, left_pre = parse(left)
    right_core, right_pre = parse(right)
    if left_core != right_core:
        return 1 if left_core > right_core else -1
    if not left_pre or not right_pre:
        return (bool(right_pre) - bool(left_pre))
    for a, b in zip(left_pre, right_pre):
        if a == b:
            continue
        if a.isdigit() and b.isdigit():
            if int(a) == int(b):
                continue
            return 1 if int(a) > int(b) else -1
        if a.isdigit() != b.isdigit():
            return -1 if a.isdigit() else 1
        return 1 if a > b else -1
    return (len(left_pre) > len(right_pre)) - (len(left_pre) < len(right_pre))


def validate_published_release(snapshot: Path, tag: str, repo: str) -> dict:
    release = json.loads(run('gh', 'api', f'repos/{repo}/releases/tags/{tag}'))
    if release.get('draft') is not False or not release.get('published_at'):
        raise ValidationError('only published Releases can synchronize main')
    if release.get('tag_name') != tag:
        raise ValidationError('Release tag mismatch')
    manifest = json.loads((snapshot / 'release-manifest.json').read_text())
    if manifest.get('version') != tag:
        raise ValidationError('source snapshot version does not match Release tag')
    actual_assets = {asset['name'] for asset in release.get('assets', [])}
    if actual_assets != set(manifest['archives']) | {'checksums.txt'}:
        raise ValidationError('Release attachment set does not match the source snapshot')
    with tempfile.TemporaryDirectory(prefix='ue5-published-assets-') as temporary:
        work = Path(temporary)
        artifacts = work / 'artifacts'
        artifacts.mkdir()
        run('gh', 'release', 'download', tag, '--repo', repo, '--dir', str(artifacts))
        plugin_manifest = work / 'plugins.json'
        plugin_manifest.write_text(json.dumps({'schema_version': 1, 'plugins': manifest['plugins']}))
        validate_dist(tag, manifest['source_ref'], plugin_manifest, artifacts, snapshot)
    return manifest


def synchronize_source(snapshot: Path, target: Path, manifest: dict) -> bool:
    """Return false for an older version; copy only owned SDK and automation paths."""
    current_path = target / 'release-manifest.json'
    if current_path.exists():
        current = json.loads(current_path.read_text())
        order = compare_versions(manifest['version'], current['version'])
        if order < 0:
            print(f"skipping older version {manifest['version']}; main is {current['version']}")
            return False
        if order == 0 and manifest != current:
            raise ValidationError('main already contains different source for this version')
    plugins = target / 'Plugins'
    if plugins.is_symlink():
        raise ValidationError('target Plugins must not be a symbolic link')
    if plugins.exists():
        shutil.rmtree(plugins)
    shutil.copytree(snapshot / 'Plugins', plugins)
    for relative in ('.gitattributes', 'release-manifest.json', *SYNC_FILES):
        source = snapshot / relative
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path, required=True)
    parser.add_argument('--target', type=Path, required=True)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--repo', required=True)
    args = parser.parse_args()
    if not VERSION_RE.fullmatch(args.tag) or not re.fullmatch(r'[\w.-]+/[\w.-]+', args.repo):
        raise ValidationError('invalid release tag or repository')
    snapshot, target = args.snapshot.resolve(), args.target.resolve()
    if run('git', 'branch', '--show-current', cwd=target) != 'main':
        raise ValidationError('target checkout must be main')
    manifest = validate_published_release(snapshot, args.tag, args.repo)
    changed = synchronize_source(snapshot, target, manifest)
    if changed:
        run('git', 'config', 'user.name', 'TapTap SDK Publisher[bot]', cwd=target)
        run('git', 'config', 'user.email', 'sdk-publisher[bot]@taptap.com', cwd=target)
        run('git', 'add', '-A', '--', '.gitattributes', 'release-manifest.json', *SYNC_FILES, cwd=target)
        run('git', 'add', '-f', '-A', '--', 'Plugins', cwd=target)
        if run('git', 'diff', '--cached', '--name-only', cwd=target):
            run('git', 'commit', '-m', f'release(UE5): sync published v{args.tag}', cwd=target)
            # Normal push preserves history and fails on concurrent main changes; rerun to retry.
            run('git', 'push', 'origin', 'HEAD:refs/heads/main', cwd=target)
        else:
            print('main already matches the published source; no commit needed')
    output_path = os.environ.get('GITHUB_OUTPUT')
    if output_path and changed:
        context = json.loads((snapshot / '.github/ue5-release-context.json').read_text())
        if context['history_env'] not in ('RND', 'PROD'):
            raise ValidationError('invalid history environment')
        visibility = run('gh', 'repo', 'view', args.repo, '--json', 'visibility', '--jq', '.visibility')
        with open(output_path, 'a', encoding='utf-8') as output:
            output.write('history=' + json.dumps(context) + '\n')
            output.write(f"is_public={'true' if visibility == 'PUBLIC' else 'false'}\n")
    summary = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary:
        with open(summary, 'a', encoding='utf-8') as output:
            output.write(f"## UE5 published source synchronization\n\n- Version: `{args.tag}`\n")
            output.write(f"- Result: {'main synchronized' if changed else 'older version skipped'}\n")


if __name__ == '__main__':
    main()
