"""Generate upgrade fingerprints from the official .56 downloads. No asset bytes are retained."""
import gzip
import hashlib
import json
from pathlib import Path
import sys
import zipfile

ARCHIVES = {
    "Initial-D-Arcade-Stage-3-Reimagined-0.3.95.56-Windows-x64.zip": "e83f192b0887ccb515a1af679f51b2e37cf778ba8cb01ad69a8975cdc9375e56",
    "Initial-D-Additional-Courses-0.3.95-community-replays.56.zip": "bd2b4271aff29522bd6a9770c490f9c783c8e1e46486bb0fcec0e4a2aa04e7d2",
}


def build(folder):
    entries = {}
    prefix = "InitialDUnity_Data/StreamingAssets/IDAS3/data/"
    for name, digest in ARCHIVES.items():
        source = folder / name
        with source.open("rb") as stream:
            if hashlib.file_digest(stream, "sha256").hexdigest() != digest:
                raise ValueError(f"Unexpected .56 release archive: {name}")
        with zipfile.ZipFile(source) as archive:
            for item in archive.infolist():
                if item.filename.startswith(prefix) and not item.is_dir():
                    with archive.open(item) as stream:
                        entry = [item.file_size, hashlib.file_digest(stream, "sha256").hexdigest()]
                    previous = entries.setdefault(item.filename[len(prefix):], entry)
                    if previous != entry:
                        raise ValueError("Inconsistent release inventory")
    output = Path(__file__).with_name("windows-56-files.json.gz")
    output.write_bytes(gzip.compress(json.dumps(entries, sort_keys=True, separators=(",", ":")).encode(), mtime=0))
    print(f"Recorded {len(entries)} stock file hashes.")


if __name__ == "__main__":
    build(Path(sys.argv[1]))
