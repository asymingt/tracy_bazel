#!/usr/bin/env python3
"""Refresh a local registry module after editing its overlay or patches.

For modules/<name>/<version>/ this:
  * copies overlay/MODULE.bazel to <version>/MODULE.bazel (BCR requires the
    two to be identical), and
  * rewrites the "overlay" and "patches" integrity maps in source.json from
    the files on disk (other keys are preserved).

Usage: tools/sync_overlay.py <name> <version> [<name> <version> ...]
       tools/sync_overlay.py --all
"""

import base64
import hashlib
import json
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MODULES = ROOT / "modules"


def integrity(path: pathlib.Path) -> str:
    digest = hashlib.sha256(path.read_bytes()).digest()
    return "sha256-" + base64.b64encode(digest).decode()


def hash_tree(base: pathlib.Path) -> dict:
    if not base.is_dir():
        return {}
    return {
        p.relative_to(base).as_posix(): integrity(p)
        for p in sorted(base.rglob("*"))
        if p.is_file()
    }


def sync(name: str, version: str) -> None:
    vdir = MODULES / name / version
    source_json = vdir / "source.json"
    source = json.loads(source_json.read_text())

    overlay_module = vdir / "overlay" / "MODULE.bazel"
    if overlay_module.exists():
        shutil.copyfile(overlay_module, vdir / "MODULE.bazel")

    overlay = hash_tree(vdir / "overlay")
    if overlay:
        source["overlay"] = overlay
    else:
        source.pop("overlay", None)

    patches = hash_tree(vdir / "patches")
    if patches:
        source["patches"] = patches
        source.setdefault("patch_strip", 1)
    else:
        source.pop("patches", None)
        source.pop("patch_strip", None)

    source_json.write_text(json.dumps(source, indent=4) + "\n")
    print(f"synced {name}@{version}: {len(overlay)} overlay, {len(patches)} patch files")


def main(argv: list) -> int:
    if argv == ["--all"]:
        pairs = [
            (v.parent.name, v.name)
            for v in sorted(MODULES.glob("*/*"))
            if (v / "source.json").exists()
        ]
    elif argv and len(argv) % 2 == 0:
        pairs = list(zip(argv[::2], argv[1::2]))
    else:
        print(__doc__, file=sys.stderr)
        return 2
    for name, version in pairs:
        sync(name, version)
    # The Bazel server caches file:// registry contents for its lifetime, so
    # changed patches (and sometimes overlays) are not picked up until restart.
    print("note: run `bazel shutdown` so the Bazel server re-reads the local registry")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
