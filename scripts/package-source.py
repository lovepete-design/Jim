#!/usr/bin/env python3
"""Package full corresponding source, vendored dependencies, docs, and demos."""
from pathlib import Path
import hashlib
import sys
import zipfile

root = Path(__file__).resolve().parent.parent
destination = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "dist" / "Triad-Audio-Source.zip"
destination.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        if relative.parts[0] in {"dist", ".git"} or relative.parts[0].startswith("build"):
            continue
        if any(part in {".git", "__pycache__", ".DS_Store"} for part in relative.parts):
            continue
        if path.is_file() and path.resolve() != destination.resolve():
            archive.write(path, Path("triad-audio") / relative)
digest = hashlib.sha256(destination.read_bytes()).hexdigest()
destination.with_suffix(destination.suffix + ".sha256").write_text(digest + "  " + destination.name + "\n")
print(f"{destination}: {destination.stat().st_size / 1048576:.1f} MiB, SHA256 {digest}")
