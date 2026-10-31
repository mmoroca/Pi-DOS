#!/usr/bin/env python3
"""Prepare a separate publication repository without changing the source Git state.

Exports current-branch history with local deployment credentials excluded, then
commits the reviewed working-tree source/documentation snapshot. Never pushes.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
EXCLUDED = {'.vscode/pro-deployer.json'}
# Explicit additions avoid accidentally publishing unrelated untracked files.
ADDITIONS = {
    '.gitattributes', '.vscode/pro-deployer.example.json',
    'docs/technical-reference.md', 'docs/github-sync.md',
    'tools/repo/prepare-github.py',
}
# A focused credential check, not a claim of exhaustive secret detection.
CREDENTIAL = re.compile(
    rb'-----BEGIN (?:OPENSSH |RSA |EC )?PRIVATE KEY-----'
    rb'|(?:ghp_|github_pat_)[A-Za-z0-9_]{20,}'
    rb'|AKIA[0-9A-Z]{16}'
    rb'''|["'](?:password|passphrase|token|secret)["']\s*:\s*["'][^"']+["']''',
    re.IGNORECASE,
)


def git(repo: Path, *args: str, data: bytes | None = None) -> bytes:
    return subprocess.run(
        ['git', '-C', str(repo), *args], input=data,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    ).stdout


def check_history(repo: Path) -> tuple[int, int]:
    """Inspect every reachable blob, reporting paths but never credential values."""
    objects = git(repo, 'rev-list', '--objects', '--all').splitlines()
    blobs = 0
    max_blob = 0
    with subprocess.Popen(
        ['git', '-C', str(repo), 'cat-file', '--batch'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    ) as reader:
        assert reader.stdin is not None and reader.stdout is not None
        for entry in objects:
            oid, _, raw_path = entry.partition(b' ')
            reader.stdin.write(oid + b'\n')
            reader.stdin.flush()
            header = reader.stdout.readline().split()
            size = int(header[2])
            content = reader.stdout.read(size)
            assert reader.stdout.read(1) == b'\n'
            if header[1] != b'blob':
                continue
            blobs += 1
            max_blob = max(max_blob, size)
            path = raw_path.decode(errors='replace')
            if path in EXCLUDED:
                raise ValueError(f'Excluded local configuration remains in history: {path}')
            if size >= 50 * 1024 * 1024:
                raise ValueError(f'Unexpected large historical blob: {path}')
            if b'\0' not in content and CREDENTIAL.search(content):
                raise ValueError(f'Potential historical credential; inspect locally: {path}')
        reader.stdin.close()
        if reader.wait() != 0:
            raise ValueError('Historical object inspection failed')
    return blobs, max_blob


def prepare(destination: Path) -> None:
    if destination.exists():
        raise ValueError(f'Refusing to overwrite an existing destination: {destination}')
    if destination == ROOT or ROOT.is_relative_to(destination):
        raise ValueError('Destination must not contain the original repository')
    branch = git(ROOT, 'symbolic-ref', '--quiet', '--short', 'HEAD').decode().strip()
    if git(ROOT, 'ls-files', '--unmerged'):
        raise ValueError('Resolve merge conflicts before preparing a snapshot')
    original_head = git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    original_count = int(git(ROOT, 'rev-list', '--count', 'HEAD'))
    remote = git(ROOT, 'remote', 'get-url', 'origin').decode().strip()
    # Use configured project identity, without changing global/local config.
    author_name = git(ROOT, 'show', '-s', '--format=%an', 'HEAD').decode().strip()
    author_email = git(ROOT, 'show', '-s', '--format=%ae', 'HEAD').decode().strip()
    for key, fallback in [('user.name', author_name), ('user.email', author_email)]:
        value = subprocess.run(
            ['git', '-C', str(ROOT), 'config', '--get', key], capture_output=True,
        ).stdout.decode().strip()
        if key == 'user.name':
            author_name = value or fallback
        else:
            author_email = value or fallback

    paths = set(git(ROOT, 'ls-files', '-z').decode().split('\0')) - {''}
    paths = (paths | ADDITIONS) - EXCLUDED
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.pidos-publication-', dir=destination.parent) as temp:
        temporary = Path(temp)
        repo = temporary / 'repo'
        original_marks = temporary / 'original.marks'
        imported_marks = temporary / 'imported.marks'
        git(ROOT, 'init', '--initial-branch', branch, str(repo))
        export_args = [
            'fast-export', f'--export-marks={original_marks}', branch, '--', '.',
            *[f':(exclude){path}' for path in sorted(EXCLUDED)],
        ]
        git(repo, 'fast-import', '--quiet', f'--export-marks={imported_marks}',
            data=git(ROOT, *export_args))
        git(repo, 'checkout', '--force', branch)
        clean_count = int(git(repo, 'rev-list', '--count', 'HEAD'))
        if clean_count != original_count:
            raise ValueError('Filtered commit count changed; review before publication')
        for relative in sorted(paths):
            source, target = ROOT / relative, repo / relative
            if source.is_symlink():
                raise ValueError(f'Review symbolic link before publication: {relative}')
            if source.is_file():
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
            elif target.is_file():
                target.unlink()
            elif relative in ADDITIONS:
                raise ValueError(f'Required preparation file missing: {relative}')
        git(repo, 'add', '--all')
        if git(repo, 'diff', '--cached', '--name-only'):
            git(repo, '-c', f'user.name={author_name}', '-c', f'user.email={author_email}',
                'commit', '-m', 'docs: add complete technical reference and prepare first GitHub sync')
        git(repo, 'remote', 'add', 'origin', remote)
        # No upstream is declared until a push actually succeeds.
        blobs, largest_blob = check_history(repo)
        if git(repo, 'status', '--porcelain'):
            raise ValueError('Prepared repository is not clean')
        git(repo, 'fsck', '--full')
        def read_marks(path: Path) -> dict[str, str]:
            return dict(line.split() for line in path.read_text().splitlines())
        before, after = read_marks(original_marks), read_marks(imported_marks)
        original_commits = set(git(ROOT, 'rev-list', 'HEAD').decode().splitlines())
        history_map = {old: after[mark] for mark, old in before.items()
                       if old in original_commits}
        report = {
            'original_head': original_head, 'branch': branch, 'origin': remote,
            'original_commits': original_count, 'preserved_development_commits': clean_count,
            'prepared_head': git(repo, 'rev-parse', 'HEAD').decode().strip(),
            'prepared_commits': int(git(repo, 'rev-list', '--count', 'HEAD')),
            'excluded_from_all_history': sorted(EXCLUDED),
            'reviewed_extra_files': sorted(ADDITIONS),
            'checked_reachable_blobs': blobs, 'largest_blob_bytes': largest_blob,
            'history_map': history_map,
            'network_actions': 'none; not pushed',
            'remaining_release_gate': 'Original-code license and third-party redistribution review',
        }
        repo.rename(destination)
    report_path = destination.with_name(destination.name + '-report.json')
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Prepared: {destination}')
    print(f'Preserved {clean_count} development commits; total {report["prepared_commits"]} commits.')
    print(f'Checked {blobs} reachable blobs; largest {largest_blob} bytes; working tree clean.')
    print(f'Original-to-publication commit map: {report_path}')
    print('Original repository/index/configuration unchanged. No network action or push performed.')
    print('Read docs/github-sync.md for publication gates and first-sync commands.')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'output/github-sync')
    args = parser.parse_args()
    try:
        prepare(args.output.resolve())
    except (ValueError, subprocess.CalledProcessError) as error:
        # Do not print captured file/credential content on failure.
        raise SystemExit(str(error)) from None


if __name__ == '__main__':
    main()
