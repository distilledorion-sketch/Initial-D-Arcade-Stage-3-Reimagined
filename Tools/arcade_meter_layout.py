"""Read-only layout reconstruction for the recovered meter widget audit.

``flatten(detail)`` uses only the generated-class WidgetTree and preserves all
switcher branches and initially hidden artwork for the live telemetry owner.
Coordinates are source pixels, x right/y down. Affines are ROW MAJOR:
    [m00,m01,m02,m10,m11,m12], x'=m00*x+m01*y+m02.
Nothing is normalized or written. ``width/height`` are the root's layout size;
``bounds`` is the unnormalized union of textured/procedural image rectangles.
Unknown cooked class defaults and material execution are not reconstructed.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

IDENTITY = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0]


def fields(value):
    return value.get("fields", {}) if isinstance(value, dict) else {}


def pair(value, default=(0.0, 0.0)):
    if isinstance(value, list) and len(value) >= 2:
        return float(value[0]), float(value[1])
    return tuple(default)


def multiply(a, b):
    """Compose a after b, using row-major 2x3 column-vector affines."""
    return [a[0]*b[0]+a[1]*b[3], a[0]*b[1]+a[1]*b[4], a[0]*b[2]+a[1]*b[5]+a[2],
            a[3]*b[0]+a[4]*b[3], a[3]*b[1]+a[4]*b[4], a[3]*b[2]+a[4]*b[5]+a[5]]


def translate(x, y):
    return [1.0, 0.0, float(x), 0.0, 1.0, float(y)]


def point(m, x, y):
    return [m[0]*x+m[1]*y+m[2], m[3]*x+m[4]*y+m[5]]


def rectangle(m, width, height):
    corners = [point(m, x, y) for x, y in ((0, 0), (width, 0), (0, height), (width, height))]
    low = [min(p[i] for p in corners) for i in range(2)]
    high = [max(p[i] for p in corners) for i in range(2)]
    return [low[0], low[1], high[0]-low[0], high[1]-low[1]]


def intersect(a, b):
    if a is None:
        return list(b)
    left, top = max(a[0], b[0]), max(a[1], b[1])
    right, bottom = min(a[0]+a[2], b[0]+b[2]), min(a[1]+a[3], b[1]+b[3])
    return [left, top, max(0.0, right-left), max(0.0, bottom-top)]


def margin(value, default=(0.0, 0.0, 0.0, 0.0)):
    f = fields(value)
    return tuple(float(f.get(k, d)) for k, d in zip(("Left", "Top", "Right", "Bottom"), default))


def render_transform(properties, width, height):
    f = fields(properties.get("RenderTransform"))
    sx, sy = pair(f.get("Scale"), (1.0, 1.0))
    shx, shy = pair(f.get("Shear"))
    tx, ty = pair(f.get("Translation"))
    angle = float(f.get("Angle", 0.0))
    px, py = pair(properties.get("RenderTransformPivot"), (0.5, 0.5))
    pivot = [px*width, py*height]
    radians = math.radians(angle)
    c, s = math.cos(radians), math.sin(radians)
    # Slate shear angles are a clockwise deviation from the normal axes;
    # negative X shear leans the tops of numerals to the right.
    shear = [1.0, math.tan(math.radians(shx)), 0.0,
             math.tan(math.radians(shy)), 1.0, 0.0]
    linear = multiply([c, -s, 0.0, s, c, 0.0], multiply(shear, [sx, 0.0, 0.0, 0.0, sy, 0.0]))
    matrix = multiply(translate(pivot[0]+tx, pivot[1]+ty), multiply(linear, translate(-pivot[0], -pivot[1])))
    return matrix, angle, pivot


def align_axis(allotted, desired, leading, trailing, alignment):
    """Slate panel slot alignment, including authored negative padding."""
    available = max(0.0, allotted-leading-trailing)
    if alignment.endswith("Fill"):
        return leading, available
    size = min(desired, available)
    if alignment.endswith("Center"):
        return (allotted-size)*0.5+leading-trailing, size
    if alignment.endswith("Right") or alignment.endswith("Bottom"):
        return allotted-size-trailing, size
    return leading, size


class _Layout:
    def __init__(self, detail):
        generated = detail.get("generatedClasses", [])
        if not generated:
            raise ValueError("Widget audit has no generated class")
        self.tree = generated[0].get("properties", {}).get("WidgetTree")
        tree = next((n for n in detail.get("widgetTreeObjects", []) if n["object"] == self.tree), None)
        if tree is None:
            raise ValueError("Generated-class WidgetTree is missing")
        self.root = tree.get("properties", {}).get("RootWidget")
        self.nodes = {n["object"]: n for n in detail.get("widgets", [])
                      if n["object"].startswith(self.tree+".")}
        if self.root not in self.nodes:
            raise ValueError("Generated-class root widget is missing")
        self.sizes = {}
        self.measuring = set()
        self.layers = []
        self.groups = []
        self.warnings = set()

    def children(self, node):
        result = []
        for order, ref in enumerate(node.get("properties", {}).get("Slots", [])):
            if ref not in self.nodes:
                raise ValueError("Unresolved panel slot: "+str(ref))
            slot = self.nodes[ref]
            child = slot.get("properties", {}).get("Content")
            if child is None:
                continue
            if child not in self.nodes:
                raise ValueError("Unresolved slot content: "+str(child))
            result.append((order, slot, self.nodes[child]))
        if node["class"].endswith(".CanvasPanel"):
            result.sort(key=lambda value: (value[1].get("properties", {}).get("ZOrder", 0), value[0]))
        return result

    @staticmethod
    def canvas(slot):
        props = slot.get("properties", {})
        f = fields(props.get("LayoutData"))
        anchor = fields(f.get("Anchors"))
        # UCanvasPanelSlot's FAnchorData default extent is 100 by 30. A
        # serialized explicit zero must remain zero, especially gauge clips.
        offsets = margin(f.get("Offsets"), (0.0, 0.0, 100.0, 30.0))
        return offsets, pair(anchor.get("Minimum")), pair(anchor.get("Maximum")), pair(f.get("Alignment")), bool(props.get("bAutoSize", False))

    def desired(self, node):
        ref = node["object"]
        if ref in self.sizes:
            return self.sizes[ref]
        if ref in self.measuring:
            raise ValueError("Cycle in widget hierarchy: "+ref)
        self.measuring.add(ref)
        p = node.get("properties", {})
        kind = node["class"].rsplit(".", 1)[-1]
        children = self.children(node)
        if kind == "Image":
            size = pair(fields(p.get("Brush")).get("ImageSize"), (32.0, 32.0))
        elif kind == "CanvasPanel":
            size = [0.0, 0.0]
            for _, slot, child in children:
                off, low, high, _, auto = self.canvas(slot)
                child_size = self.desired(child) if auto else off[2:]
                for axis in range(2):
                    docked = low[axis] == high[axis] and low[axis] in (0.0, 1.0)
                    size[axis] = max(size[axis], child_size[axis]+(abs(off[axis]) if docked else 0.0))
        elif kind == "HorizontalBox":
            sizes = []
            for _, slot, child in children:
                l, t, r, b = margin(slot.get("properties", {}).get("Padding"))
                w, h = self.desired(child)
                sizes.append((w+l+r, h+t+b))
            size = [sum(s[0] for s in sizes), max((s[1] for s in sizes), default=0.0)]
        else:
            relevant = children
            if kind == "WidgetSwitcher" and children:
                active = int(p.get("ActiveWidgetIndex", 0))
                relevant = [children[min(max(active, 0), len(children)-1)]]
            sizes = []
            for _, slot, child in relevant:
                l, t, r, b = margin(slot.get("properties", {}).get("Padding"))
                w, h = self.desired(child)
                sizes.append((w+l+r, h+t+b))
            size = [max((s[0] for s in sizes), default=0.0), max((s[1] for s in sizes), default=0.0)]
            if kind == "SizeBox":
                for axis, dimension in enumerate(("Width", "Height")):
                    # Cooked legacy archetypes sometimes retain an override
                    # value with its enable bit absent. Honor only true bits.
                    if p.get("bOverride_"+dimension+"Override", False):
                        size[axis] = float(p.get(dimension+"Override", size[axis]))
                    if p.get("bOverride_MinDesired"+dimension, False):
                        size[axis] = max(size[axis], float(p.get("MinDesired"+dimension, 0.0)))
                    if p.get("bOverride_MaxDesired"+dimension, False):
                        size[axis] = min(size[axis], float(p.get("MaxDesired"+dimension, size[axis])))
            if kind not in ("Overlay", "SizeBox", "ScaleBox", "WidgetSwitcher", "RetainerBox", "Border"):
                self.warnings.add("Unrecognized container uses maximum child extent: "+kind)
        self.measuring.remove(ref)
        self.sizes[ref] = tuple(max(0.0, float(v)) for v in size)
        return self.sizes[ref]

    def panel_rect(self, slot, desired, width, height):
        p = slot.get("properties", {})
        l, t, r, b = margin(p.get("Padding"))
        x, w = align_axis(width, desired[0], l, r, p.get("HorizontalAlignment", "HAlign_Fill"))
        y, h = align_axis(height, desired[1], t, b, p.get("VerticalAlignment", "VAlign_Fill"))
        return x, y, w, h

    def arrange(self, node, width, height):
        p = node.get("properties", {})
        kind = node["class"].rsplit(".", 1)[-1]
        children = self.children(node)
        cursor = 0.0
        fixed = 0.0
        weights = []
        if kind == "HorizontalBox":
            for _, slot, child in children:
                sp = slot.get("properties", {})
                sz = fields(sp.get("Size"))
                fill = str(sz.get("SizeRule", "Automatic")).endswith("Fill")
                weight = float(sz.get("Value", 1.0)) if fill else 0.0
                weights.append(weight)
                l, _, r, _ = margin(sp.get("Padding"))
                fixed += l+r+(0.0 if fill else self.desired(child)[0])
        for index, (order, slot, child) in enumerate(children):
            wanted = self.desired(child)
            scale = (1.0, 1.0)
            if kind == "CanvasPanel":
                off, low, high, alignment, auto = self.canvas(slot)
                size = list(wanted if auto else off[2:])
                pos = [0.0, 0.0]
                for axis, extent in enumerate((width, height)):
                    if low[axis] != high[axis]:
                        size[axis] = max(0.0, extent*(high[axis]-low[axis])-off[axis]-off[axis+2])
                        pos[axis] = extent*low[axis]+off[axis]
                    else:
                        pos[axis] = extent*low[axis]+off[axis]-alignment[axis]*size[axis]
                rect = (*pos, *size)
            elif kind == "HorizontalBox":
                sp = slot.get("properties", {})
                l, _, r, _ = margin(sp.get("Padding"))
                share = max(0.0, width-fixed)*weights[index]/sum(weights) if weights[index] and sum(weights) else wanted[0]
                allocated = share+l+r
                x, y, w, h = self.panel_rect(slot, wanted, allocated, height)
                rect = (cursor+x, y, w, h)
                cursor += allocated
            elif kind == "ScaleBox":
                # This source uses ScaleToFit unless explicitly serialized.
                stretch = str(p.get("Stretch", "ScaleToFit")).rsplit("::", 1)[-1]
                sx = width/wanted[0] if wanted[0] else 1.0
                sy = height/wanted[1] if wanted[1] else 1.0
                factor = max(sx, sy) if stretch == "ScaleToFill" else sx if stretch == "ScaleToFitX" else sy if stretch == "ScaleToFitY" else min(sx, sy)
                if stretch == "None":
                    factor = 1.0
                if stretch == "UserSpecified":
                    factor = float(p.get("UserSpecifiedScale", 1.0))
                direction = str(p.get("StretchDirection", "Both"))
                if direction.endswith("DownOnly"):
                    factor = min(1.0, factor)
                elif direction.endswith("UpOnly"):
                    factor = max(1.0, factor)
                scale = (sx, sy) if stretch == "Fill" else (factor, factor)
                sp = slot.get("properties", {})
                ax = sp.get("HorizontalAlignment", "HAlign_Center")
                ay = sp.get("VerticalAlignment", "VAlign_Center")
                x = (width-wanted[0]*scale[0])*(1.0 if ax.endswith("Right") else 0.0 if ax.endswith("Left") else 0.5)
                y = (height-wanted[1]*scale[1])*(1.0 if ay.endswith("Bottom") else 0.0 if ay.endswith("Top") else 0.5)
                rect = (x, y, *wanted)
            else:
                rect = self.panel_rect(slot, wanted, width, height)
            yield slot, child, rect, scale, order

    def walk(self, node, width, height, before, ancestors, clips, slot=None, layout=None, switchers=()):
        p = node.get("properties", {})
        local, angle, pivot = render_transform(p, width, height)
        transform = multiply(before, local)
        clip_rect = None
        for clip in clips:
            clip_rect = intersect(clip_rect, clip["rect"])
        mode = str(p.get("Clipping", ""))
        if "ClipToBounds" in mode or "ClipToBoundsAlways" in mode:
            bounds = rectangle(transform, width, height)
            clip = {"source": node["object"], "name": node["name"], "rect": bounds,
                    "transform": list(transform), "width": width, "height": height}
            clips = clips+(clip,)
            clip_rect = intersect(clip_rect, bounds)
            if abs(transform[1]) > 1e-5 or abs(transform[3]) > 1e-5:
                self.warnings.add("Rotated clip requires its retained polygon, not only clipRect: "+node["name"])
        record = {"name": node["name"], "source": node["object"], "width": width, "height": height,
                  "transform": transform, "transform6": transform, "parentTransform": list(before),
                  "localTransform": local, "angle": angle, "pivotX": pivot[0], "pivotY": pivot[1],
                  "properties": p, "slot": slot["object"] if slot else None,
                  "slotProperties": slot.get("properties", {}) if slot else {},
                  "layout": list(layout or (0.0, 0.0, width, height)), "clipRect": clip_rect}
        kind = node["class"].rsplit(".", 1)[-1]
        if kind == "Image":
            brush = fields(p.get("Brush"))
            record.update({"resource": brush.get("ResourceObject"), "imageProperties": p,
                           "parentGroups": list(ancestors), "clips": list(clips),
                           "switchers": list(switchers), "bounds": rectangle(transform, width, height)})
            self.layers.append(record)
            return
        self.groups.append(record)
        for child_slot, child, rect, scale, order in self.arrange(node, width, height):
            x, y, w, h = rect
            child_before = multiply(transform, [scale[0], 0.0, x, 0.0, scale[1], y])
            selections = switchers
            if kind == "WidgetSwitcher":
                selections += ({"source": node["object"], "name": node["name"], "index": order,
                                "activeIndex": int(p.get("ActiveWidgetIndex", 0))},)
            self.walk(child, w, h, child_before, ancestors+(record,), clips, child_slot, rect, selections)


def flatten(widget_detail):
    """Return ordered image layers, ancestor records, source extents and clips.

    Caller owns resource resolution, initial/event visibility, live animation,
    clipping shader behavior, meter placement and any final bounding-box shift.
    ``parentTransform`` includes placement/ancestor transforms, excluding the
    image's own RenderTransform. ``pivotX/Y`` are LOCAL PIXELS, not fractions.
    """
    layout = _Layout(widget_detail)
    root = layout.nodes[layout.root]
    width, height = layout.desired(root)
    if width <= 0 or height <= 0:
        raise ValueError("Nonpositive natural meter extent: "+layout.root)
    layout.walk(root, width, height, list(IDENTITY), (), ())
    drawable = [layer["bounds"] for layer in layout.layers if layer["resource"]]
    bounds = [0.0, 0.0, width, height]
    if drawable:
        left, top = min(r[0] for r in drawable), min(r[1] for r in drawable)
        right, bottom = max(r[0]+r[2] for r in drawable), max(r[1]+r[3] for r in drawable)
        bounds = [left, top, right-left, bottom-top]
    return {"width": width, "height": height, "bounds": bounds, "root": layout.root,
            "layers": layout.layers, "groups": layout.groups, "warnings": sorted(layout.warnings)}


def verify_audit(audit_root):
    """No output files: validate registry coverage and authored layout anchors."""
    root = Path(audit_root)
    registry = json.loads((root/"meter-registry.json").read_text(encoding="utf-8"))
    results = {}
    for row in registry["rows"]:
        name = row["selectedClass"].rsplit("/", 1)[-1].split(".", 1)[0]
        detail = json.loads((root/"widgets-details"/(name+".json")).read_text(encoding="utf-8"))
        result = flatten(detail)
        expected = detail["primaryImageCount"]
        assert len(result["layers"]) == expected, (row["row"], len(result["layers"]), expected)
        assert len({layer["source"] for layer in result["layers"]}) == expected
        for layer in result["layers"]:
            assert all(math.isfinite(v) for v in layer["transform"])
            # Meter77 contains authored negative Canvas dimensions on its
            # thunder effects. Keep signed geometry instead of discarding it.
            assert math.isfinite(layer["width"]) and math.isfinite(layer["height"])
        results[int(row["row"])] = result
    meter = results[31]
    by_name = {layer["name"]: layer for layer in meter["layers"]}
    centers = {name: point(layer["transform"], layer["width"]/2, layer["height"]/2)
               for name, layer in by_name.items()}
    origin = centers["BaseTexture"]
    expected = {"CenterPin": (0, -21), "RightPin": (210, 3),
                "SpeedRate03": (-47, 107), "SpeedRate02": (-10, 107), "SpeedRate01": (27, 107)}
    for name, offset in expected.items():
        actual = [centers[name][i]-origin[i] for i in range(2)]
        assert all(abs(actual[i]-offset[i]) < 1e-4 for i in range(2)), (name, actual, offset)
    for id_, size in ((38, (576, 370)), (66, (576, 380)), (67, (613, 350)), (76, (576, 370))):
        assert (results[id_]["width"], results[id_]["height"]) == size, (id_, results[id_]["width"], results[id_]["height"])
    assert len(results) == 87 and not {16, 33, 59}.intersection(results)
    return {"meters": len(results), "images": sum(len(r["layers"]) for r in results.values()),
            "meter31CentersRelativeToBase": {name: [centers[name][i]-origin[i] for i in range(2)] for name in expected},
            "warningCount": sum(len(r["warnings"]) for r in results.values())}


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("audit_root", help="Recovered Catalog/Meter Audit directory")
    print(json.dumps(verify_audit(parser.parse_args().audit_root), indent=2))
