#!/usr/bin/env python3
"""Fetch and verify locked upstream sources. Requires Python 3.12 or newer."""

import argparse
import hashlib
import json
from pathlib import Path
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def extract_sources(package, sources, members):
    # Buildroot includes absolute links in its target filesystem skeleton.
    # Create these last so archive extraction can never follow them on the host.
    absolute_links = []

    def source_filter(member, destination):
        if member.issym() and Path(member.linkname).is_absolute():
            safe = tarfile.data_filter(member.replace(linkname="."), destination)
            absolute_links.append(safe.replace(linkname=member.linkname))
            return None
        return tarfile.data_filter(member, destination)

    package.extractall(sources, members=members, filter=source_filter)
    for member in absolute_links:
        path = sources / member.name
        if not path.parent.resolve().is_relative_to(sources.resolve()):
            raise tarfile.FilterError(f"symlink outside source directory: {member.name}")
        path.symlink_to(member.linkname)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true", help="use cached archives only")
    parser.add_argument("--component", action="append", choices=("sdl2", "dosbox-x", "buildroot"),
                        help="fetch only selected components (default: Phase 0 sources)")
    args = parser.parse_args()
    if sys.version_info < (3, 12):
        parser.error("Python 3.12 or newer is required for safe archive extraction")
    lock = json.loads((ROOT / "sources.lock.json").read_text())
    cache = ROOT / "dl"
    sources = ROOT / "output/src"
    cache.mkdir(exist_ok=True)
    sources.mkdir(parents=True, exist_ok=True)
    for name in args.component or ("sdl2", "dosbox-x"):
        entry = lock[name]
        archive = cache / entry["archive"]
        if not archive.exists():
            if args.offline:
                raise RuntimeError(f"missing cached archive: {archive}")
            temporary = archive.with_suffix(".part")
            print(f"Downloading {name} @ {entry['revision']}", flush=True)
            try:
                with urllib.request.urlopen(entry["url"], timeout=60) as response:
                    with temporary.open("wb") as output:
                        while block := response.read(1024 * 1024):
                            output.write(block)
                if digest(temporary) != entry["sha256"]:
                    raise RuntimeError(f"SHA-256 mismatch for {name}")
                temporary.replace(archive)
            finally:
                temporary.unlink(missing_ok=True)
        if digest(archive) != entry["sha256"]:
            raise RuntimeError(f"SHA-256 mismatch: {archive}; remove it and fetch again")
        destination = sources / entry["directory"]
        marker = destination / ".pi-dos-source-sha256"
        if not marker.exists():
            # Never overwrite an unknown source tree or local source changes.
            if destination.exists():
                raise RuntimeError(f"unmanaged source tree: {destination}; move it aside first")
            with tarfile.open(archive, "r:gz") as package:
                members = package.getmembers()
                if any(Path(m.name).parts[0] != entry["directory"] for m in members):
                    raise RuntimeError(f"unexpected archive root for {name}")
                extract_sources(package, sources, members)
            marker.write_text(entry["sha256"] + "\n")
        elif marker.read_text().strip() != entry["sha256"]:
            raise RuntimeError(f"source stamp mismatch: {destination}")
        print(f"Verified {name}: {entry['revision']}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, tarfile.TarError) as error:
        sys.exit(str(error))
