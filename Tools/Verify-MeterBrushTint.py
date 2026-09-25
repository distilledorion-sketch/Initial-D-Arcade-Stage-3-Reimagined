#!/usr/bin/env python3
"""Verify retained source brush colors without importing or changing assets."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

spec = importlib.util.spec_from_file_location("meter_importer", Path(__file__).with_name("Import-ArcadeMeters.py"))
importer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(importer)


def run(source, catalog=None):
    audit = source / "Catalog/Meter Audit"
    registry = json.loads((audit / "meter-registry.json").read_text(encoding="utf-8"))["rows"]
    specified, style_dependent = [], []
    color_paths = {}
    brush_tracks = []
    for row in registry:
        name = row["selectedClass"].rsplit("/", 1)[-1].split(".", 1)[0]
        detail = json.loads((audit / "widgets-details" / (name + ".json")).read_text(encoding="utf-8"))
        for track in detail["tracks"]:
            properties = track.get("properties", {})
            path = str(properties.get("PropertyPath", ""))
            property_name = str(properties.get("PropertyName", ""))
            if "Color" in track["class"]:
                color_paths[path] = color_paths.get(path, 0) + 1
            if any(word in path or word in property_name for word in ("Brush", "Tint")):
                brush_tracks.append(track["object"])
        tree = detail["generatedClasses"][0]["properties"]["WidgetTree"] + "."
        for widget in detail["widgets"]:
            if not widget["object"].startswith(tree) or not widget["class"].endswith(".Image"):
                continue
            brush = widget["properties"].get("Brush", {}).get("fields", {})
            result = importer.brush_tint(brush)
            tint = brush.get("TintColor", {}).get("fields", {})
            if not tint:
                assert result == {}
                continue
            record = {"meter": int(row["row"]), "name": widget["name"], "source": widget["object"]}
            if "SpecifiedColor" in tint:
                expected = tint["SpecifiedColor"]["values"]
                assert result == {"brushColor": expected}, record
                specified.append({**record, "rgba": expected})
            else:
                assert "brushColor" not in result, record
                assert result.get("brushColorUseRule") == tint["ColorUseRule"], record
                style_dependent.append({**record, "rule": tint["ColorUseRule"]})

    assert len(registry) == 87 and len(specified) == 103 and len(style_dependent) == 55
    assert not brush_tracks, "Animated brush tint needs a separate runtime color owner"
    assert all(0 <= channel <= 1 for item in specified for channel in item["rgba"])
    steampunk = {item["name"]: item["rgba"] for item in specified if item["meter"] == 66}
    expected = {"NixeAdd": [1, .2625199854373932, 0, 1],
                "coil_add_1": [1, .2625199854373932, 0, 1],
                "coil_add": [1, .1799429953098297, 0, .800000011920929],
                "RevLamp1": [1, 1, 1, .43809500336647034]}
    for name, rgba in expected.items():
        assert steampunk[name] == rgba, name
    if catalog:
        imported = json.loads(catalog.read_text(encoding="utf-8"))["meters"]
        layers = {(meter["id"], layer["name"]): layer for meter in imported for layer in meter["layers"]}
        for item in specified:
            assert layers[item["meter"], item["name"]].get("brushColor") == item["rgba"], item
        for item in style_dependent:
            layer = layers[item["meter"], item["name"]]
            assert "brushColor" not in layer and layer.get("brushColorUseRule") == item["rule"], item
    return {"meterCount": 87, "specifiedTintCount": len(specified),
            "specifiedTintMeterIDs": sorted({item["meter"] for item in specified}),
            "unresolvedStyleDependentCount": len(style_dependent),
            "unresolvedStyleDependentMeterIDs": sorted({item["meter"] for item in style_dependent}),
            "specifiedTints": specified, "styleDependentTints": style_dependent,
            "colorAnimationPropertyCounts": color_paths, "brushTintAnimationTracks": brush_tracks,
            "specifiedColorChannelsOutsideUnitRange": 0,
            "catalogVerified": catalog is not None,
            "catalogSha256": hashlib.sha256(catalog.read_bytes()).hexdigest() if catalog else None,
            "scope": "Actual source SpecifiedColor values checked against importer extraction. Foreground uses inherited style; no local RGB is guessed. No texture bytes changed."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("D:/Initial D games/Extracted HUD Assets - The Arcade S3"))
    parser.add_argument("--catalog", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = run(args.source, args.catalog)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: result[key] for key in ("meterCount", "specifiedTintCount", "specifiedTintMeterIDs", "unresolvedStyleDependentCount", "unresolvedStyleDependentMeterIDs", "catalogVerified")}))
