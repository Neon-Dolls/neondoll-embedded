#!/usr/bin/env python3
"""Apply NeonDoll's isolated patch set to the verified upstream v0.4.8 archive."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tarfile

REVISION = "9b7660fed27151abdcbf32b43861044cf4a32497"
ARCHIVE_SHA256 = "1c0839424dd01c7a89e51242f1377c0b7b683338c40303c0cb93f06815d3bc6a"
ALLOWED = {"include/esp_wireguard.h", "src/esp_wireguard.c", "src/wireguardif.c", "src/neondoll_ip6.h"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("destination", type=Path, help="new or empty directory; use basename wireguard for IDF")
    args = parser.parse_args()
    if hashlib.sha256(args.archive.read_bytes()).hexdigest() != ARCHIVE_SHA256:
        parser.error("archive SHA-256 mismatch; use the exact pinned revision")
    destination = args.destination.resolve()
    if destination.exists() and any(destination.iterdir()):
        parser.error("destination must be empty; existing sources will not be overwritten")
    originals = {}
    prefix = f"wireguard-{REVISION}"
    with tarfile.open(args.archive) as archive:
        members = archive.getmembers()
        for member in members:
            path = Path(member.name)
            if path.is_absolute() or ".." in path.parts or path.parts[0] != prefix or not (member.isdir() or member.isfile()):
                parser.error(f"unsafe archive member: {member.name}")
        destination.mkdir(parents=True, exist_ok=True)
        for member in members:
            path = Path(*Path(member.name).parts[1:])
            if member.isfile():
                data = archive.extractfile(member).read()
                target = destination / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                originals[str(path)] = hashlib.sha256(data).digest()
    patch_dir = Path(__file__).resolve().parent
    for name in (patch_dir / "series").read_text().splitlines():
        if name and not name.startswith("#"):
            patch = str(patch_dir / name)
            subprocess.run(["git", "apply", "--check", patch], cwd=destination, check=True)
            subprocess.run(["git", "apply", patch], cwd=destination, check=True)
    changed = set()
    files = {str(p.relative_to(destination)) for p in destination.rglob("*") if p.is_file()}
    for name in files | originals.keys():
        path = destination / name
        digest = hashlib.sha256(path.read_bytes()).digest() if path.is_file() else None
        if originals.get(name) != digest:
            changed.add(name)
    if changed != ALLOWED:
        parser.error(f"unexpected patch delta: {sorted(changed)}; crypto sources must remain unchanged")
    print(f"Prepared {REVISION} + NeonDoll IPv6 patch; only {sorted(changed)} changed.")


if __name__ == "__main__":
    main()
