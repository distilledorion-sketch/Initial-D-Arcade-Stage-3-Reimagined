"""Create independently installable ZIPs from the canonical ranked track inventory."""
import argparse
import hashlib
import os
from pathlib import Path
import tempfile
import zipfile

from disc_extras import map_packs, member, ranked_files


def package(maps, destination, files):
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination) as temporary:
        for pack in sorted(map_packs(files)):
            target = destination / f"Initial-D-Track-{pack}.zip"
            stage = Path(temporary) / target.name
            with zipfile.ZipFile(stage, "w", zipfile.ZIP_DEFLATED, compresslevel=5) as archive:
                for name, (size, checksum) in sorted(files.items()):
                    if name.partition("/")[0] != pack:
                        continue
                    source = member(maps, name)
                    digest = hashlib.sha256()
                    with source.open("rb") as data, archive.open(name, "w") as output:
                        for chunk in iter(lambda: data.read(1024 * 1024), b""):
                            digest.update(chunk)
                            output.write(chunk)
                        if data.tell() != size or digest.hexdigest() != checksum:
                            raise ValueError(f"Track file differs from the ranked version: {name}")
            os.replace(stage, target)
            print(target.name, flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("maps", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    package(args.maps, args.destination, ranked_files(Path(__file__).resolve().parents[2]))
