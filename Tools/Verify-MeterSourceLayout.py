#!/usr/bin/env python3
"""Read source layouts; verify Overlay defaults and audit all 87 geometry deltas.

Never imports or writes game assets. --report writes only the requested JSON.
--catalog optionally verifies an already regenerated catalog's Miku resources.
"""
import argparse
import json
from pathlib import Path

import arcade_meter_layout as layout


def legacy_panel_rect(self, slot, desired, width, height):
    p = slot.get("properties", {})
    left, top, right, bottom = layout.margin(p.get("Padding"))
    x, w = layout.align_axis(width, desired[0], left, right, p.get("HorizontalAlignment", "HAlign_Fill"))
    y, h = layout.align_axis(height, desired[1], top, bottom, p.get("VerticalAlignment", "VAlign_Fill"))
    return x, y, w, h


def geometry(layer):
    return {key: layer[key] for key in ("width", "height", "transform")}


def run(source, catalog=None):
    audit = source / "Catalog/Meter Audit"
    proof = layout.verify_audit(audit)
    current = layout._Layout.panel_rect
    # Distinguish omitted defaults, explicit Fill, and other slot classes.
    slot = {"class": "/Script/UMG.OverlaySlot", "properties": {}}
    assert current(None, slot, (20, 30), 100, 100) == (0, 0, 20, 30)
    slot["properties"] = {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"}
    assert current(None, slot, (20, 30), 100, 100) == (0, 0, 100, 100)
    slot = {"class": "/Script/UMG.SizeBoxSlot", "properties": {}}
    assert current(None, slot, (20, 30), 100, 100) == (0, 0, 100, 100)

    changes = []
    rows = json.loads((audit / "meter-registry.json").read_text(encoding="utf-8"))["rows"]
    for row in rows:
        name = row["selectedClass"].rsplit("/", 1)[-1].split(".", 1)[0]
        detail = json.loads((audit / "widgets-details" / (name + ".json")).read_text(encoding="utf-8"))
        after = layout.flatten(detail)
        try:
            layout._Layout.panel_rect = legacy_panel_rect
            before = layout.flatten(detail)
        finally:
            layout._Layout.panel_rect = current
        assert len(before["layers"]) == len(after["layers"])
        delta = []
        for a, b in zip(before["layers"], after["layers"]):
            assert a["source"] == b["source"]
            av = [a["width"], a["height"], *a["transform"]]
            bv = [b["width"], b["height"], *b["transform"]]
            if any(abs(x-y) > 1e-5 for x, y in zip(av, bv)):
                delta.append({"name": b["name"], "source": b["source"],
                              "before": geometry(a), "after": geometry(b)})
        if delta:
            changes.append({"meter": int(row["row"]), "layers": delta})

    # These paired soft references are consecutive constructor arguments in
    # Setup_AddDayChangeTexture_SoftRef, not the raw Meter49 template brush.
    day = "/Game/IND/UI/Race/Meter/00/Texture/T_Meter00_PointRmp_A.T_Meter00_PointRmp_A"
    night = day.replace("PointRmp_A", "PointRmp_B")
    pair = b"\x67\x1f" + day.encode() + b"\0\x67\x1f" + night.encode() + b"\0"
    bindings = []
    for meter in (71, 72, 73):
        path = source / f"Raw/GameProject/Content/IND/UI/Race/Meter/{meter}/WBP_Meter_{meter}.uexp"
        data = path.read_bytes()
        assert data.count(pair) == 1, (meter, "constructor day/night arguments changed")
        bindings.append({"meter": meter, "day": day, "night": night,
                         "uexpPairedArgumentsOffset": data.index(pair)})
    if catalog:
        imported = json.loads(catalog.read_text(encoding="utf-8"))["meters"]
        for meter in imported:
            if meter["id"] in (71, 72, 73):
                pin = next(layer for layer in meter["layers"] if layer["name"] == "CenterPin")
                assert pin["texture"].endswith("/00/Texture/T_Meter00_PointRmp_A")
                assert pin["nightTexture"].endswith("/00/Texture/T_Meter00_PointRmp_B")
                assert pin["runtimeResource"] == day
                assert "/49/" in pin["sourceResource"]  # preserve template provenance
    return {"verification": proof, "changedMeterCount": len(changes),
            "changedImageCount": sum(len(item["layers"]) for item in changes),
            "changes": changes, "runtimeNeedleBindings": bindings}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("D:/Initial D games/Extracted HUD Assets - The Arcade S3"))
    parser.add_argument("--report", type=Path)
    parser.add_argument("--catalog", type=Path)
    args = parser.parse_args()
    result = run(args.source, args.catalog)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: result[key] for key in ("changedMeterCount", "changedImageCount")}))
    print("Changed meters:", ", ".join(str(item["meter"]) for item in result["changes"]))
