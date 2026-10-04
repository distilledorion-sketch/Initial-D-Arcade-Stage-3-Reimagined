#!/usr/bin/env python3
"""Import the original credits/final-card bank without altering its layout."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import shutil
import extract_original_hud as hud


def export(hostfs, output):
    with tempfile.TemporaryDirectory(prefix="idas3-ending-") as temporary:
        hud.BANKS = [("ending", "ending")]
        hud.export(hostfs, Path(temporary))
        source = Path(temporary) / "ending"
        output.mkdir(parents=True, exist_ok=True)
        for relative in ("ending.idasmesh", "textures/textures.idastex"):
            target = output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / relative, target)
        manifest = json.loads((source / "manifest.json").read_text())
        for entry in manifest["source_files"].values():
            entry["path"] = str(Path(entry["path"]).relative_to(hostfs)).replace("\\", "/")
        manifest["outputs"] = {relative: hashlib.sha256((output / relative).read_bytes()).hexdigest()
                               for relative in ("ending.idasmesh", "textures/textures.idastex")}
        manifest["runtime"] = {"owner": "0C0EB2C0", "scroll": "0C0EBBC0",
                               "scroll_table": "0C262738", "final_card": "0C0EBD00",
                               "music_stream": 12,
                               "scope": "Original credits, photo strips and final artwork. Driving cinematic is not included."}
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hostfs", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    export(args.hostfs, args.out)
