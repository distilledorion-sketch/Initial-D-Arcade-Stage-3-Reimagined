#!/usr/bin/env python3
"""Export the original HLecture banks; no emulator or guest execution."""
from pathlib import Path
import argparse
import json
import extract_original_hud as hud


def export(hostfs: Path, out: Path):
    # HLecture Init 18ADC6 and course path selectors 290420..2904BC.
    names = ("myogi", "usui", "akagi", "akina", "happo", "iroha", "syomaru", "tuchizaka")
    hud.BANKS = [("lecture", "lecture")] + [("lecmap", "lecmap_" + name) for name in names]
    result = hud.export(hostfs, out)
    result["source_owner"] = "HLecture 18AAC0 / Init18ACE0 / Main18D880"
    result["source_paths"] = "2903D8,2903FC,290420..2904BC"
    result["scope"] = "Original assets only; no claim that all original telemetry analysis is implemented."
    (out / "index.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--hostfs", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(export(args.hostfs, args.out), indent=2))
