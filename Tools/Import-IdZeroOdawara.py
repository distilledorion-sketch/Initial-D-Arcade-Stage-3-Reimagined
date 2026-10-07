"""Convert the authored IDZero 2.20 Odawara circuit to a D3 imported-course pack.

Odawara is a two-lap circuit whose directions take different roads through
the corner before the line, so IDZero ships a centre path, edge paths and a
collision mesh per direction. The pack keeps both: `odawara*` for
counterclockwise and `odawara_reverse*`/`odawara-reverse.rcl` for clockwise. Collision repacking and
its coarse-cell repair are shared with Import-IdZeroGunsai.py.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import shutil
import struct
from pathlib import Path
from idas_efo import Scene, placements

_spec = importlib.util.spec_from_file_location("idzero_gunsai", Path(__file__).with_name("Import-IdZeroGunsai.py"))
gunsai = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gunsai)

SOURCE = "odawara"
LAPS = 2  # COURSE_DATA.ini NumLaps
# COURSE_DATA.ini's suffix for the clockwise files, then the pack's path and
# collision names (`<slug>-reverse.rcl` is the loader's existing convention).
DIRECTIONS = (("", "odawara", "odawara"), ("_b", "odawara_reverse", "odawara-reverse"))
Point = tuple[float, float, float]


def edge_sections(center: list[Point], cloud: list[Point]) -> list[Point]:
    """Order an IDZero edge point set into one cross-section per centre point.

    Odawara's road_l/road_r files hold half the centre's point count in mesh
    order, not route order. Every point lies beside exactly one stretch of the
    closed centre path, so its projection onto that path orders it, and the
    edge at centre point i is the ordered polyline at parameter i."""
    count = len(center)
    buckets: dict[tuple[int, int], list[int]] = {}
    for i, (x, _, z) in enumerate(center):
        buckets.setdefault((math.floor(x / 32), math.floor(z / 32)), []).append(i)
    ordered = []
    for point in cloud:
        bx, bz = math.floor(point[0] / 32), math.floor(point[2] / 32)
        best = None
        for i in {j for dx in (-1, 0, 1) for dz in (-1, 0, 1) for k in buckets.get((bx + dx, bz + dz), ())
                  for j in ((k - 1) % count, k)}:
            a, b = center[i], center[(i + 1) % count]
            ex, ez = b[0] - a[0], b[2] - a[2]
            t = max(0.0, min(1.0, ((point[0] - a[0]) * ex + (point[2] - a[2]) * ez) / (ex * ex + ez * ez)))
            distance = math.hypot(point[0] - a[0] - t * ex, point[2] - a[2] - t * ez)
            if best is None or distance < best[0]:
                best = (distance, i + t)
        if best is None or best[0] > 20:
            raise ValueError(f"Odawara edge point {point} is not beside the road")
        ordered.append((best[1], point))
    ordered.sort()
    # One period either side, so every centre parameter has two neighbours.
    ring = ([(s - count, p) for s, p in ordered[-2:]] + ordered +
            [(s + count, p) for s, p in ordered[:2]])
    out, j = [], 0
    for i in range(count):
        while ring[j + 1][0] < i:
            j += 1
        (s0, p0), (s1, p1) = ring[j], ring[j + 1]
        if not s0 <= i <= s1 or s1 - s0 > 8:
            raise ValueError(f"Odawara edge has no points beside path {i}")
        u = (i - s0) / (s1 - s0) if s1 > s0 else 0.0
        out.append(tuple(p0[k] + u * (p1[k] - p0[k]) for k in range(3)))
    return out


def check_cross_sections(center: list[Point], left: list[Point], right: list[Point]) -> float:
    """Require each edge on its own side and advancing with the closed centre; return the widest section."""
    count = len(center)
    sides = set()
    for i in range(count):
        a, b = center[i - 1], center[(i + 1) % count]
        tx, tz = b[0] - a[0], b[2] - a[2]
        sides.add(tuple((e[i][0] - center[i][0]) * -tz + (e[i][2] - center[i][2]) * tx > 0 for e in (left, right)))
        step = center[(i + 1) % count][0] - center[i][0], center[(i + 1) % count][2] - center[i][2]
        for e, name in ((left, "left"), (right, "right")):
            n = e[(i + 1) % count]
            if (n[0] - e[i][0]) * step[0] + (n[2] - e[i][2]) * step[1] <= 0:
                raise ValueError(f"Odawara {name} edge folds back at path {i}")
        width = math.dist(left[i], right[i])
        if not 0.05 < width < 200 or math.dist(center[i], left[i]) > width:
            raise ValueError(f"Invalid Odawara road width at {i}")
    if len(sides) != 1 or next(iter(sides))[0] == next(iter(sides))[1]:
        raise ValueError("Odawara road edges change or share sides")
    return max(math.dist(l, r) for l, r in zip(left, right))


def verify_road_contact(collision: Path, center: list[Point], left: list[Point],
                        right: list[Point]) -> tuple[int, list[int]]:
    """Exercise RCL1 coarse lookup and linked triangles around the whole lap.

    The centre path runs through the toll-booth island at path 1778-1800,
    where the road passes either side; there both lanes must be driveable.
    Returns (samples checked, samples whose centre is off the road)."""
    data = collision.read_bytes()
    h = struct.unpack_from("<12I", data)
    vertices = [struct.unpack_from("<3f", data, h[5] + i * 32) for i in range(h[4])]
    triangles = [struct.unpack_from("<8h", data, h[7] + i * 16) for i in range(h[6])]
    cells = [struct.unpack_from("<13fh", data, h[11] + i * 56) for i in range(h[10])]

    def driveable(point: Point) -> bool:
        x, y, z = point
        candidates = [(gunsai.cell_distance(cell, (x, y + 1, z)), cell[13]) for cell in cells]
        candidates = [(distance, seed) for distance, seed in candidates if distance >= 0]
        if not candidates:
            return False
        found = gunsai.walk_triangles(vertices, triangles, min(candidates)[1], x, z)[0]
        return found >= 0 and triangles[found][7] >= 0 and abs(
            sum(vertices[v][1] for v in triangles[found][:3]) / 3 - y) <= 3

    checked, divided = 0, []
    for index in range(0, len(center), 4):
        if not driveable(center[index]):
            lanes = [tuple((c + e) / 2 for c, e in zip(center[index], edge[index])) for edge in (left, right)]
            if not all(map(driveable, lanes)):
                raise ValueError(f"Road path leaves the driveable collision at {index}")
            divided.append(index)
        checked += 1
    return checked, divided


def export_area_fog(course: Path, output: Path) -> None:
    """Convert the day scene's area fog keys (COURSE_DATA.ini AreaFogPath).

    AFG v4: 16-byte header (magic, version, key count, global block size), a
    global block, then 48-byte keys of twelve floats: path point, fog start
    and end distance, colour, density, and specular/noise terms. course_p.fx
    blends towards the colour by density * (1-(1-t)^2) over start..end; its
    Brownian noise is not reproduced, so only those five values are kept."""
    data = (course / "env" / "odawara_day_dry.afg").read_bytes()
    magic, version, count, block = struct.unpack_from("<4s3I", data)
    if magic != b"AFG\0" or version != 4 or len(data) != 16 + block + count * 48:
        raise ValueError("Unexpected area fog file")
    keys = []
    for index in range(count):
        point, start, end, r, g, b, density = struct.unpack_from("<7f", data, 16 + block + index * 48)
        keys.append({"point": round(point, 3), "start": start, "end": end, "density": round(density, 4),
                     "color": [round(r, 4), round(g, 4), round(b, 4)]})
    (output / "area-fog.json").write_text(json.dumps({"keys": keys}, indent=2) + "\n")


def export(source: Path, output: Path) -> None:
    course = source / "data" / "COURSE" / SOURCE
    route = course / "path"
    if not route.is_dir():
        raise FileNotFoundError(route)
    output.mkdir(parents=True, exist_ok=True)
    report = {"source": "Initial D Arcade Stage Zero 2.20 / Odawara", "laps": LAPS, "directions": {}}
    sources = []
    forward: list[list[Point]] = []
    for suffix, path_name, collision_name in DIRECTIONS:
        paths = [route / f"odawara_path_road{suffix}_{side}.pa4" for side in ("c", "l", "r")]
        collision = course / "collision" / f"odawara_collision{suffix}.bin"
        sources += [*paths, collision]
        center, left_cloud, right_cloud = (gunsai.path_points(path) for path in paths)
        gap = math.dist(center[0], center[-1])
        if not 0.5 < gap < gunsai.MAX_LINK or any(
                math.dist(a, b) > gunsai.MAX_LINK for a, b in zip(center, center[1:])):
            raise ValueError("Odawara centre path is not one closed lap")
        left, right = edge_sections(center, left_cloud), edge_sections(center, right_cloud)
        widest = check_cross_sections(center, left, right)
        # D3 closes a circuit by repeating its first point (Course::closed),
        # which makes the lap period the source point count.
        roads = [road + [road[0]] for road in (center, left, right)]
        for points, side in zip(roads, ("", "_l", "_r")):
            gunsai.write_path(output / f"{path_name}_path{side}.bin", points)
        if not forward:
            forward = roads
        # IDZero's direction-selected blocking classes (see the Gunsai
        # converter) are kept for parity; neither Odawara mesh uses them.
        blocking = gunsai.BLOCKING_CLASS[bool(suffix)]
        name = f"{collision_name}.rcl"
        variant = gunsai.convert_collision(collision, output / name, blocking, roads[0])
        variant["blockingClass"] = blocking
        variant["roadSamplesChecked"], variant["dividedRoadSamples"] = verify_road_contact(
            output / name, center, left, right)
        report["directions"]["clockwise" if suffix else "counterclockwise"] = {
            "lapPoints": len(center), "closingGap": round(gap, 3),
            "edgePoints": [len(left_cloud), len(right_cloud)],
            "widestCrossSection": round(widest, 3), "collision": {name: variant}}
        print(f"Odawara{suffix}: {len(center)} lap points, {variant['triangles']} collision triangles", flush=True)
    count = len(forward[0])
    (output / "road.bin").write_bytes(b"HKR1" + struct.pack("<I", count) + b"".join(
        struct.pack("<3f", *point) for road in forward for point in road))
    # Circuit race data: lap count, then D3 adaptation allowances for the
    # start and each further half lap, to be tuned with driving.
    (output / "race.bin").write_bytes(b"HKC1" + struct.pack("<5i", LAPS, 90, 65, 65, 65))
    lamps = placements(route / "odawara_path_light.pa8")
    (output / "lamps.bin").write_bytes(b"HKL1" + struct.pack("<I", len(lamps)) +
                                      b"".join(struct.pack("<3f", *row[:3]) for row in lamps))
    (output / "course.id").write_text("17\n")
    report["sourceSha256"] = {str(path.relative_to(source)).replace("\\", "/"):
                              hashlib.sha256(path.read_bytes()).hexdigest() for path in sources}
    (output / "driving-import.json").write_text(json.dumps(report, indent=2) + "\n")
    export_area_fog(course, output)
    # Day uses Akina Lake's shader configuration, as COURSE_DATA.ini does.
    for condition, prefix, lighting in (("day_dry", "day_dry", "akinaLake_day_dry"),
                                        ("night_dry", "ngt_dry", "odawara_ngt_dry")):
        destination = output if condition == "day_dry" else output / condition
        scene = Scene(destination, "Odawara")
        scene.archive(course / "efo" / f"odawara_{prefix}_crs_a.efo")
        scene.archive(course / "efo" / f"odawara_{prefix}_crs_m.efo")
        scene.archive(course / "efo" / f"odawara_{prefix}_sky.efo", "sky")
        trees = scene.archive(course / "efo" / f"odawara_{prefix}_tree.efo", "tree")
        scene.trees(route / "odawara_path_tree.pa8", trees)
        scene.save(course / "env" / f"{lighting}_shader_config.ybo", count)
        if destination != output:
            shutil.copyfile(output / "road.bin", destination / "road.bin")
    # Wet races: Tools/Add-IdZeroWetLook.py RuntimeAssets/ODAWARA


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True,
                        help="IDZero package root containing data/COURSE")
    parser.add_argument("--output", type=Path, required=True,
                        help="New Odawara pack directory")
    arguments = parser.parse_args()
    export(arguments.source, arguments.output)
