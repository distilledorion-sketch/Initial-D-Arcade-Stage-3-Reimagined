#!/usr/bin/env python3
"""Import the 87 registry-selected recovered Arcade S3 meter compositions.

The source is read-only. PNG/HDR bytes are copied unchanged. Geometry is
reconstructed from the generated-class UMG tree, not contact-sheet images.
Cooked material operations that were stripped remain explicit limitations.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import uuid

from arcade_meter_layout import flatten

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = Path("D:/Initial D games/Extracted HUD Assets - The Arcade S3")
GUID_NAMESPACE = uuid.UUID("7f75e250-9db1-4f2d-a979-691106676671")
FRAME_MAXIMUMS = {1: 8000, 2: 8000, 3: 9000, 4: 9000, 5: 10000, 6: 10000, 7: 10000, 8: 13000}


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, separators=(",", ":"), allow_nan=False) + "\n", encoding="utf-8")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fields(value):
    return value.get("fields", {}) if isinstance(value, dict) else {}


def rgba(value, fallback=(1, 1, 1, 1)):
    if isinstance(value, dict):
        return list(value.get("values", [value.get(k, fallback[i]) for i, k in enumerate("RGBA")]))
    return list(fallback)


def stable_guid(path):
    return uuid.uuid5(GUID_NAMESPACE, path.relative_to(ROOT).as_posix()).hex


def ensure_meta(path, texture_template=None):
    target = Path(str(path) + ".meta")
    if target.exists():
        return
    if path.is_dir():
        value = "fileFormatVersion: 2\nguid: %s\nfolderAsset: yes\nDefaultImporter:\n  externalObjects: {}\n  userData: \n  assetBundleName: \n  assetBundleVariant: \n" % stable_guid(path)
    elif texture_template is not None:
        value = re.sub(r"(?m)^guid: .*", "guid: " + stable_guid(path), texture_template)
        if path.suffix.lower() == ".hdr":
            value = value.replace("sRGBTexture: 1", "sRGBTexture: 0")
    else:
        value = "fileFormatVersion: 2\nguid: %s\nTextScriptImporter:\n  externalObjects: {}\n  userData: \n  assetBundleName: \n  assetBundleVariant: \n" % stable_guid(path)
    target.write_text(value, encoding="utf-8")


class Importer:
    def __init__(self, source, output, verification, copy_assets=True):
        self.source, self.output, self.verification = source, output, verification
        self.audit = source / "Catalog/Meter Audit"
        self.material_data = read_json(self.audit / "materials.json")
        self.materials = self.material_data["materials"]
        self.registry = read_json(self.audit / "meter-registry.json")["rows"]
        self.names = read_json(ROOT / "Tools/ArcadeMeterNames.json")
        self.texture_records = []
        self.texture_paths = {}
        self.texture_sources = {}
        self.missing_textures = set()
        self.copy_assets = copy_assets
        self.texture_template = (ROOT / "Assets/Resources/ArcadeHud/Meter31/T_Meter31_LampEf_03.png.meta").read_text()

    def import_textures(self):
        for reference, entry in sorted(self.material_data["textures"].items()):
            outputs = entry.get("conversion", {}).get("outputs", [])
            supported = [p for p in outputs if Path(p).suffix.lower() in (".png", ".hdr")]
            if not supported:
                self.missing_textures.add(reference)
                continue
            relative = Path(supported[0])
            source = self.source / "Exports" / relative
            if not source.is_file():
                raise FileNotFoundError(source)
            target = self.output / "Textures" / relative
            resource = target.relative_to(ROOT / "Assets/Resources").with_suffix("").as_posix()
            self.texture_paths[reference] = resource
            self.texture_sources[reference] = source
            target.parent.mkdir(parents=True, exist_ok=True)
            source_hash = digest(source)
            if self.copy_assets and (not target.exists() or digest(target) != source_hash):
                shutil.copyfile(source, target)
            if not target.exists() or digest(target) != source_hash:
                raise ValueError("Texture copy mismatch: " + str(target))
            ensure_meta(target, self.texture_template)
            self.texture_records.append({"object": reference, "source": str(source).replace("\\", "/"),
                "destination": target.relative_to(ROOT).as_posix(), "sha256": source_hash,
                "bytes": source.stat().st_size, "byteIdentical": True})
        for folder in sorted({self.output, *(p for p in self.output.rglob("*") if p.is_dir())}, key=str):
            ensure_meta(folder)

    def texture(self, reference):
        if not reference:
            return ""
        result = self.texture_paths.get(reference, "")
        if not result:
            self.missing_textures.add(reference)
        return result

    def import_alpha_bounds(self):
        # This metadata reads original alpha only; exported image bytes remain
        # unchanged. Pillow is used for PNG decoding, never image rewriting.
        from PIL import Image
        bounds = []
        by_name = collections.defaultdict(list)
        for record in self.texture_records:
            path = ROOT / record["destination"]
            resource = path.relative_to(ROOT / "Assets/Resources").with_suffix("").as_posix()
            if path.suffix.lower() == ".hdr":
                rect = (0, 0, 1, 1)
            else:
                with Image.open(path) as original:
                    alpha = original.convert("RGBA").getchannel("A")
                    box = alpha.point(lambda value: 255 if value > 1 else 0).getbbox()
                    rect = (box[0] / original.width, box[1] / original.height,
                        (box[2] - box[0]) / original.width, (box[3] - box[1]) / original.height) if box else (0, 0, 0, 0)
            entry = dict(zip(("x", "y", "width", "height"), rect))
            entry.update(texture=resource, name=path.stem)
            bounds.append(entry)
            by_name[path.stem].append(entry)
        conflicts = []
        for name, entries in by_name.items():
            if len(entries) <= 1:
                continue
            nonempty = [e for e in entries if e["width"] > 0 and e["height"] > 0]
            left = min((e["x"] for e in nonempty), default=0)
            top = min((e["y"] for e in nonempty), default=0)
            right = max((e["x"] + e["width"] for e in nonempty), default=0)
            bottom = max((e["y"] + e["height"] for e in nonempty), default=0)
            conflicts.append({"name": name, "textures": [e["texture"] for e in entries],
                "x": left, "y": top, "width": right - left, "height": bottom - top})
        target = self.output / "alpha-bounds.json"
        write_json(target, {"version": 1, "alphaThresholdExclusive": 1 / 255,
            "textures": bounds, "basenameUnions": conflicts})
        ensure_meta(target)
        return {"textureCount": len(bounds), "emptyCount": sum(e["width"] == 0 or e["height"] == 0 for e in bounds),
            "basenameConflictCount": len(conflicts), "sha256": digest(target)}

    def material(self, reference):
        m = self.materials.get(reference)
        if m is None:
            return {"texture": self.texture(reference), "material": "", "materialParent": "", "parameters": [], "textureBindings": [], "vectorParameters": [], "additive": False}, None
        effective = m.get("effectiveParameters", {})
        bindings = [{"name": key, "texture": self.texture(value.get("value"))} for key, value in effective.get("texture", {}).items() if value.get("value")]
        by_name = {b["name"]: b["texture"] for b in bindings}
        primary = next((by_name[n] for n in ("Texture", "BaseTex", "Texture01", "Tex_Base00", "Color_Texture", "Mask_01") if by_name.get(n)), "")
        if not primary:
            primary = next((b["texture"] for b in bindings if b["texture"]), "")
        parameters = [{"name": key, "value": value["value"]} for key, value in effective.get("scalar", {}).items() if isinstance(value.get("value"), (int, float))]
        vectors = [{"name": key, "values": rgba(value["value"], (0, 0, 0, 0))} for key, value in effective.get("vector", {}).items() if value.get("value") is not None]
        parent = m.get("parentChain", [reference])[-1]
        flags = m.get("blendAndDomainEvidence", [])
        additive = any(item.get("BlendMode") == "BLEND_Additive" for item in flags)
        return {"texture": primary, "material": reference, "materialParent": parent,
            "parameters": parameters, "textureBindings": bindings, "vectorParameters": vectors, "additive": additive}, m

    def variants(self, texture, role):
        if not texture:
            return [], ""
        source_ref = next((key for key, value in self.texture_paths.items() if value == texture), "")
        pattern = re.match(r"^(.*_Rmp)(\d\d)_([AB])$", Path(texture).name, re.I)
        variants = []
        if pattern and (role in ("static", "rpm") or "Meter" in Path(texture).name):
            prefix = texture.rsplit("/", 1)[0] + "/" + pattern.group(1)
            for resource in self.texture_paths.values():
                m = re.fullmatch(re.escape(prefix) + r"(\d\d)_([AB])", resource)
                if m:
                    index = int(m.group(1))
                    variants.append({"texture": resource, "index": index, "day": m.group(2), "state": "", "maxRpm": FRAME_MAXIMUMS.get(index, 10000)})
        elif role == "transmission":
            base = re.sub(r"_0[12]$", "", texture)
            for suffix, state in (("_01", "manual"), ("_02", "automatic")):
                # Retro Wave's recovered labels invert the common convention:
                # its _01 artwork reads AT and its _02 artwork reads MT.
                if Path(base).name == "T_Meter76_BaseMission":
                    state = "automatic" if suffix == "_01" else "manual"
                resource = base + suffix
                if resource in self.texture_paths.values():
                    variants.append({"texture": resource, "index": 0, "day": "", "state": state, "maxRpm": 0})
        night = texture[:-2] + "_B" if texture.endswith("_A") else ""
        if night not in self.texture_paths.values():
            night = ""
        return sorted(variants, key=lambda v: (v["maxRpm"], v["index"], v["day"])), night

    @staticmethod
    def animation_role(name):
        low = name.lower()
        if "centerpin" in low:
            return "rpm"
        if "leftpin" in low or "speedpin" in low:
            return "speed"
        if "accel" in low:
            return "accel"
        if "brake" in low:
            return "brake"
        if "driftlamp" in low:
            return "drift"
        if "revlamp" in low or "overrev" in low:
            return "rev"
        if "lowlamp" in low or "shiftdown" in low:
            return "low"
        return ""

    def curves(self, detail):
        result = collections.defaultdict(list)
        sections = {s["object"]: s for s in detail["sections"]}
        for animation in detail["animations"]:
            animation_times = [time for binding in animation.get("trackBindings", [])
                for reference in binding.get("sections", [])
                for channel in sections[reference].get("serializedChannels", [])
                for time in (channel.get("times") or {}).get("values", [])]
            animation_start = min(animation_times, default=0)
            animation_end = max(animation_times, default=0)
            # Slot names repeat in different panels. Animation GUIDs retain the
            # owning widget name, including the clipped pedal gauges in meter76.
            owner_by_guid = {item.get("AnimationGuid", {}).get("serializedHex"): item.get("WidgetName", "")
                for item in animation.get("properties", {}).get("AnimationBindings", {}).get("items", [])}
            for binding in animation.get("trackBindings", []):
                target = owner_by_guid.get(binding.get("targetGuid", {}).get("serializedHex")) or binding["target"]
                for ref in binding.get("sections", []):
                    section = sections[ref]
                    for channel in section.get("serializedChannels", []):
                        path = channel["path"]
                        parameter = ""
                        match = re.search(r"\.(Scalar|Color)ParameterNamesAndCurves\.items\[(\d+)\]", path)
                        if match:
                            items = section["properties"][match[1] + "ParameterNamesAndCurves"]["items"]
                            parameter = items[int(match[2])]["ParameterName"]
                        properties = {".Rotation": "Rotation", ".Translation": "Translation.X", ".Translation[1]": "Translation.Y",
                            ".Scale": "Scale.X", ".Scale[1]": "Scale.Y", ".Shear": "Shear.X", ".Shear[1]": "Shear.Y",
                            ".RedCurve": "Color.R", ".GreenCurve": "Color.G", ".BlueCurve": "Color.B", ".AlphaCurve": "Color.A",
                            ".ByteCurve": "Visibility", ".RightCurve": "Layout.Right", ".FloatCurve": binding.get("propertyPath") or "Value"}
                        prop = properties.get(path)
                        if prop is None:
                            prop = "Parameter"
                            for suffix, value in (("RedCurve", "Color.R"), ("GreenCurve", "Color.G"), ("BlueCurve", "Color.B"), ("AlphaCurve", "Color.A")):
                                if path.endswith(suffix):
                                    prop = value
                        times = (channel.get("times") or {}).get("values", [])
                        data = channel.get("values")
                        values = data.get("keyValues", []) if isinstance(data, dict) else data or []
                        default = channel.get("default")
                        if not values and default is None:
                            continue
                        record = {"animation": animation["name"], "parameter": parameter, "property": prop,
                            "animationStart": animation_start, "animationEnd": animation_end,
                            "times": times, "values": values, "defaultValue": default if isinstance(default, (int, float)) else 0,
                            "hasDefault": default is not None, "sourceOffset": section["serialOffsetInUexp"], "owner": None}
                        result[target].append(record)
        return result

    def role(self, name, curves):
        if name in ("SpeedRate01", "SpeedRate02", "SpeedRate03"):
            return {"SpeedRate01": "speed1", "SpeedRate02": "speed10", "SpeedRate03": "speed100"}[name]
        if re.fullmatch(r"RPM0[1-5]", name):
            return "rpm" + str(10 ** (int(name[-1]) - 1))
        if name == "GearRate01":
            return "gear"
        if name == "CarMode":
            return "transmission"
        if "Drift" in name and "Corner" not in name:
            return "drift"
        if name.startswith("RevLamp") or name == "Rev_Over":
            return "low" if name == "RevLamp2" else "rev"
        for curve in curves:
            if curve.get("owner") is not None:
                continue
            role = self.animation_role(curve["animation"])
            if role == "drift" and not (curve["property"].startswith("Color.") or curve["property"] == "RenderOpacity"):
                continue
            if role:
                return role
        if "Accel" in name and any(s in name for s in ("Pin", "Gauge", "Gauge_")):
            return "accel"
        if "Brake" in name and any(s in name for s in ("Pin", "Gauge", "Gaguge")):
            return "brake"
        return "static"

    @staticmethod
    def owner(group):
        result = {k: group.get(k, fallback) for k, fallback in (("name", ""), ("transform", []), ("width", 0), ("height", 0))}
        result["pivotX"] = group.get("pivotX", 0) / group["width"] if group.get("width") else .5
        result["pivotY"] = group.get("pivotY", 0) / group["height"] if group.get("height") else .5
        p = group.get("properties", {})
        result.update(Importer.transform_baseline(p))
        result["opacity"] = p.get("RenderOpacity", 1)
        result["colorAlpha"] = rgba(p.get("ContentColorAndOpacity"))[3]
        result["clipRect"] = group.get("clipRect") or []
        result["clipsToBounds"] = "ClipToBounds" in str(p.get("Clipping", ""))
        return result

    @staticmethod
    def transform_baseline(properties):
        t = fields(properties.get("RenderTransform"))
        scale, shear, translation = t.get("Scale", [1, 1]), t.get("Shear", [0, 0]), t.get("Translation", [0, 0])
        return {"angle": t.get("Angle", 0), "scaleX": scale[0], "scaleY": scale[1],
            "shearX": shear[0], "shearY": shear[1], "translationX": translation[0], "translationY": translation[1]}

    def import_meter(self, row):
        widget_name = row["selectedClass"].split("/")[-1].split(".")[0]
        detail = read_json(self.audit / "widgets-details" / (widget_name + ".json"))
        geometry = flatten(detail)
        all_curves = self.curves(detail)
        source_name = next(v["text"] for k, v in row["properties"].items() if k.startswith("Name_") and isinstance(v, dict))
        result = {"id": int(row["row"]), "name": self.names[row["row"]], "sourceNameJapanese": source_name,
            "sourceClass": row["selectedClass"], "width": geometry["width"], "height": geometry["height"], "layers": [],
            "limitations": list(geometry.get("warnings", []))}
        for item in geometry["layers"]:
            name = item["name"]
            props = item["imageProperties"]
            brush = fields(props.get("Brush"))
            source_resource = brush.get("ResourceObject", "")
            mat, material = self.material(source_resource)
            curves = [dict(c) for c in all_curves.get(name, [])]
            ancestors = item.get("parentGroups", [])
            for group in ancestors:
                for curve in all_curves.get(group["name"], []):
                    copy = dict(curve)
                    copy["owner"] = self.owner(group)
                    curves.append(copy)
            role = self.role(name, curves)
            if role == "static" and any(g["name"] == "DriftLampColor" for g in ancestors):
                role = "drift"
            color = rgba(props.get("ColorAndOpacity"))
            opacity = float(props.get("RenderOpacity", 1))
            inherited_visible = True
            inherited_retainer = []
            for group in ancestors:
                p = group.get("properties", {})
                opacity *= p.get("RenderOpacity", 1)
                if p.get("Visibility", "").endswith(("Hidden", "Collapsed")):
                    inherited_visible = False
                if p.get("EffectMaterial"):
                    effect, _ = self.material(p["EffectMaterial"])
                    inherited_retainer.append({"material": p["EffectMaterial"], "owner": self.owner(group),
                        "materialParent": effect["materialParent"], "additive": effect["additive"],
                        "parameters": effect["parameters"], "textureBindings": effect["textureBindings"]})
                    for binding in effect["textureBindings"]:
                        if binding["name"] != "Texture":
                            mat["textureBindings"].append(binding)
            scalars = {p["name"]: p["value"] for p in mat["parameters"]}
            cols, rows = max(1, int(scalars.get("Colums", 1))), max(1, int(scalars.get("Row", 1)))
            index = int(scalars.get("Index", 0))
            uv = [index % cols / cols, 1 - (index // cols + 1) / rows, 1 / cols, 1 / rows] if rows * cols > 1 else [0, 0, 1, 1]
            transform = list(item["transform"])
            # Signed Canvas sizes encode mirroring in meter77. Preserve the
            # geometry with positive local sizes and reflected affine columns.
            if item["width"] < 0:
                transform[0] *= -1
                transform[3] *= -1
            if item["height"] < 0:
                transform[1] *= -1
                transform[4] *= -1
            layer = {"name": name, **mat, "role": role, "x": transform[2], "y": transform[5],
                "width": abs(item["width"]), "height": abs(item["height"]),
                "pivotX": item.get("pivotX", 0) / item["width"] if item["width"] else .5,
                "pivotY": item.get("pivotY", 0) / item["height"] if item["height"] else .5,
                "angle": item.get("angle", fields(props.get("RenderTransform")).get("Angle", 0)), "angleMin": 0, "angleMax": 0,
                "transform": transform, "parentTransform": item.get("parentTransform", []), "color": color, "opacity": opacity,
                "ownOpacity": float(props.get("RenderOpacity", 1)),
                "visibility": props.get("Visibility", "Visible") if inherited_visible else "Hidden", "clipRect": item.get("clipRect") or [],
                # All 21 recovered two-row gear atlases reserve cell zero for
                # zero/neutral or a blank; their numbered gears use cells 1-6.
                "atlasCols": cols, "atlasRows": rows, "digitOffset": 0,
                "uv": uv, "curves": curves, "parents": [self.owner(g) for g in ancestors], "retainers": inherited_retainer,
                "disabledReason": "", "sourceResource": source_resource, "sourceWidget": item.get("source", "")}
            layer.update(self.transform_baseline(props))
            layer["switchers"] = item.get("switchers", [])
            layer["textureVariants"], layer["nightTexture"] = self.variants(layer["texture"], role)
            rotation = next((c for c in curves if c["property"] == "Rotation" and self.animation_role(c["animation"]) == role and c["values"]), None)
            if rotation:
                layer["angleMin"], layer["angleMax"] = rotation["values"][0], rotation["values"][-1]
            if "CornerSpeed" in name or any("CornerSpeed" in g["name"] for g in ancestors):
                layer["disabledReason"] = "Corner-entry/exit speed telemetry is not available in this port."
            elif "AudioVisualizer" in name or "AudioCapture" in mat["materialParent"]:
                layer["disabledReason"] = "Audio-spectrum render target requires live FFT and capture; no fabricated animation."
            elif role == "drift" and (any(c in name for c in ("Blue", "Orange", "Red"))
                    or any(g["name"] in ("Blue", "Orange", "Red") for g in ancestors)):
                layer["disabledReason"] = "Only the source green drift color is enabled; severity colors are not inferred."
            elif "GearRateEffect" in name or "GearRate01_Blur" in name or "GearRate_Roll" in name or "SpeedRateEffect" in name:
                layer["disabledReason"] = "Transient gear/speed event effect requires source event state; retained but not fabricated."
            elif not layer["texture"]:
                layer["disabledReason"] = "No resolved texture: layout-only widget or opaque procedural material."
            elif "Add_Ball" in mat["materialParent"]:
                layer["disabledReason"] = "Procedural source material has no retained executable graph."
            if material and any(key in mat["materialParent"] for key in ("Homography", "AudioCapture", "Aura2", "UVScroll", "EffRotation", "LedMotion")):
                result["limitations"].append("%s: cooked %s effect graph is not fully reconstructable." % (name, mat["materialParent"].split("/")[-1]))
            result["layers"].append(layer)
        result["limitations"] = sorted(set(result["limitations"]))
        return result

    def run(self):
        self.output.mkdir(parents=True, exist_ok=True)
        self.import_textures()
        alpha_bounds = self.import_alpha_bounds()
        meters = [self.import_meter(row) for row in self.registry]
        meters.sort(key=lambda m: m["id"])
        assert len(meters) == 87 and len({m["id"] for m in meters}) == 87
        for meter in meters:
            if not meter["width"] > 0 or not meter["height"] > 0:
                raise ValueError("Invalid meter bounds: " + str(meter["id"]))
            roles = {layer["role"] for layer in meter["layers"] if not layer["disabledReason"]}
            if not {"speed1", "speed10", "speed100"}.issubset(roles):
                raise ValueError("Missing functional speed digits: " + str(meter["id"]))
        write_json(self.output / "catalog.json", {"version": 1, "meters": meters})
        ensure_meta(self.output / "catalog.json")
        report = {"sourceRoot": str(self.source).replace("\\", "/"), "registrySha256": digest(self.audit / "meter-registry.json"),
            "catalogSha256": digest(self.output / "catalog.json"), "meterCount": len(meters),
            "alphaBounds": alpha_bounds,
            "pngCount": sum(Path(t["source"]).suffix.lower() == ".png" for t in self.texture_records),
            "hdrCount": sum(Path(t["source"]).suffix.lower() == ".hdr" for t in self.texture_records),
            "textureBytes": sum(t["bytes"] for t in self.texture_records), "textures": self.texture_records,
            "unresolvedTextureReferences": sorted(self.missing_textures),
            "meters": [{"id": m["id"], "name": m["name"], "sourceClass": m["sourceClass"], "width": m["width"], "height": m["height"],
                "layerCount": len(m["layers"]), "roles": dict(collections.Counter(l["role"] for l in m["layers"] if not l["disabledReason"])),
                "disabled": [{"name": l["name"], "reason": l["disabledReason"]} for l in m["layers"] if l["disabledReason"]],
                "limitations": m["limitations"]} for m in meters],
            "notes": ["Only registry-selected generated widget trees are imported; legacy unsuffixed00-15 are excluded.",
                "Copied original converted PNG and HDR bytes are SHA256-verified. No contact sheets or generated artwork.",
                "Source color/visibility/animation channels are preserved; stripped shader operators and unavailable event inputs remain limitations.",
                "Frame indices01/02=8000,03/04=9000,05/06/07=10000,08=13000 follow recovered scale families; per-car red-zone selection remains a port choice."]}
        write_json(self.verification / "import-manifest.json", report)
        print(json.dumps({k: report[k] for k in ("meterCount", "pngCount", "hdrCount", "textureBytes", "unresolvedTextureReferences")}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=ROOT / "Assets/Resources/ArcadeHud/Catalog")
    parser.add_argument("--verification", type=Path, default=ROOT / "Verification/hud-all-meters-20260924")
    parser.add_argument("--no-copy", action="store_true", help="Rebuild JSON and verify already-copied textures without copying them.")
    args = parser.parse_args()
    Importer(args.source, args.output, args.verification, not args.no_copy).run()


if __name__ == "__main__":
    main()
