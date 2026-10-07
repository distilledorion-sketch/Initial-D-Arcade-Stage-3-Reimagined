"""Convert the authored IDZero 2.20 Gunsai road and collision to D3 pack data.

This is the driving-data stage of the import. It deliberately does not create a
menu bank or scenery: those require the separate YABX EFO conversion.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import shutil
from pathlib import Path
from idas_efo import Scene, placements


SOURCE = "gunsai"
CHECKPOINTS = (93, 1054, 1738, 2429, 3094)


def path_points(path: Path) -> list[tuple[float, float, float]]:
    data = path.read_bytes()
    if data[:5] != b"PathP" or len(data) < 32:
        raise ValueError(f"Not a PathP file: {path}")
    count = struct.unpack_from("<I", data, 12)[0]
    if count < 2 or len(data) != 32 + count * 16:
        raise ValueError(f"Invalid PathP count or size: {path}")
    points = [struct.unpack_from("<3f", data, 32 + i * 16) for i in range(count)]
    if not all(math.isfinite(v) for point in points for v in point):
        raise ValueError(f"Nonfinite PathP coordinate: {path}")
    return points


# Path spacing is ~2 units. Point 0 of every Gunsai path wraps to 2 units past
# the last point, ~120 units from point 1, and points 1-5 are a stray stub at
# y=0 ~56 units from point 6; export replaces them, and longer links are
# ignored for cross-sections.
MAX_LINK = 10.0


def attach_start(points: list[tuple[float, float, float]]) -> bool:
    """Extend the road back over a detached head before the first checkpoint.

    IDZero's point 0 closes the path onto its far end and points 1-5 lie off
    the road. Each point before the last detached link becomes one path step
    before its successor. Indices stay stable for checkpoints, gates and
    lighting events; the HUD map otherwise draws the head as a road branch."""
    detached = [i for i in range(CHECKPOINTS[0]) if math.dist(points[i], points[i + 1]) > MAX_LINK]
    if not detached:
        return False
    for i in range(detached[-1], -1, -1):
        points[i] = tuple(2 * a - b for a, b in zip(points[i + 1], points[i + 2]))
    return True


def tangent(center: list[tuple[float, float, float]], i: int) -> tuple[float, float] | None:
    """Unit XZ tangent at i from its neighbours, ignoring detached links; None if detached."""
    a = center[i - 1] if i > 0 and math.dist(center[i - 1], center[i]) <= MAX_LINK else center[i]
    b = center[i + 1] if i + 1 < len(center) and math.dist(center[i], center[i + 1]) <= MAX_LINK else center[i]
    tx, tz = b[0] - a[0], b[2] - a[2]
    length = math.hypot(tx, tz)
    return (tx / length, tz / length) if length else None


def align_edge(center: list[tuple[float, float, float]],
               edge: list[tuple[float, float, float]]) -> tuple[list[tuple[float, float, float]], int]:
    """Resample an IDZero edge path onto the centre path's cross-sections.

    IDZero's road_l/road_r paths have the centre's point count but their own
    spacing: point i drifts up to ~25 points along the road from centre point i.
    D3 builds its race-path cells from left[i]/centre[i]/right[i] as one
    cross-section, and folded cells lose the car's progress. Each output point
    is where the centre's XZ perpendicular meets the edge polyline, searched in
    a window that follows the edge. A hit much wider than the previous section
    (another leg of the edge) or none falls back to the closest edge point.
    Returns (points, fallbacks).
    """
    out = [edge[0]]
    j, previous, fallbacks = 0, math.dist(center[0], edge[0]), 0
    for i in range(1, len(center)):
        direction = tangent(center, i)
        if direction is None:
            raise ValueError(f"Gunsai centre path has no direction at {i}")
        nx, nz = -direction[1], direction[0]
        cx, cz = center[i][0], center[i][2]
        window = range(max(j - 40, 0), min(j + 150, len(edge) - 1))
        best = None
        for k in window:
            p, q = edge[k], edge[k + 1]
            ex, ez = q[0] - p[0], q[2] - p[2]
            det = ex * nz - nx * ez
            if abs(det) < 1e-9:
                continue
            dx, dz = p[0] - cx, p[2] - cz
            s, u = (ex * dz - ez * dx) / det, (nx * dz - nz * dx) / det
            if -1e-6 <= u <= 1 + 1e-6 and (best is None or abs(s) < abs(best[0])):
                best = (s, k, u)
        if best is not None and abs(best[0]) <= max(previous * 1.6, previous + 4):
            s, j, u = best
            p, q = edge[j], edge[j + 1]
            point = tuple(p[t] + u * (q[t] - p[t]) for t in range(3))
        else:
            fallbacks += 1

            def closest(k: int) -> tuple[tuple[float, ...], int]:
                p, q = edge[k], edge[k + 1]
                e = [q[t] - p[t] for t in range(3)]
                u = sum((center[i][t] - p[t]) * e[t] for t in range(3)) / (sum(v * v for v in e) or 1)
                u = max(0.0, min(1.0, u))
                return tuple(p[t] + u * e[t] for t in range(3)), k

            point, j = min((closest(k) for k in window), key=lambda r: math.dist(r[0], center[i]))
        out.append(point)
        previous = math.dist(center[i], point)
    return out, fallbacks


def check_cross_sections(center: list[tuple[float, float, float]],
                         left: list[tuple[float, float, float]],
                         right: list[tuple[float, float, float]]) -> float:
    """Require each edge on its own side and advancing with the centre; return the widest section."""
    sides = []
    for i in range(len(center)):
        direction = tangent(center, i)
        if direction is None:
            continue
        tx, tz = direction
        sides.append(tuple((e[i][0] - center[i][0]) * -tz + (e[i][2] - center[i][2]) * tx > 0
                           for e in (left, right)))
        if i + 1 < len(center) and math.dist(center[i], center[i + 1]) <= MAX_LINK:
            for e, name in ((left, "left"), (right, "right")):
                step = center[i + 1][0] - center[i][0], center[i + 1][2] - center[i][2]
                if (e[i + 1][0] - e[i][0]) * step[0] + (e[i + 1][2] - e[i][2]) * step[1] <= 0:
                    raise ValueError(f"Gunsai {name} edge folds back at path {i}")
    if len(set(sides)) != 1 or sides[0][0] == sides[0][1]:
        raise ValueError("Gunsai road edges change or share sides")
    return max(math.dist(l, r) for l, r in zip(left, right))


def write_path(path: Path, points: list[tuple[float, float, float]]) -> None:
    path.write_bytes(struct.pack("<II", len(points), 3) + b"".join(
        struct.pack("<3f", *point) for point in points))


# IDZero's wall trace also stops at one attribute class chosen by direction:
# R64top (0x40) forward, R80btm (0x50) backward (IndRun 0x1800457F0,
# 0x180048CA4). On Gunsai these are road-level strips behind each start.
BLOCKING_CLASS = {False: 0x40, True: 0x50}


def convert_collision(source: Path, destination: Path, blocking_class: int,
                      center: list[tuple[float, float, float]]) -> dict[str, int]:
    """Repack RCL2's shared geometry into the D3 solver's RCL1 layout.

    RCL2 adds cached 128-byte triangle records and a 4-byte vertex table after
    the coarse cells. Its first four sections retain RCL1 geometry and seeds.
    The eight RCL2 triangle dwords are the three vertex IDs, three neighbor
    IDs, packed material/surface flags, and a reserved zero. RCL1 stores these
    same fields as eight signed halfwords. D3 has no direction-selected class,
    so triangles of `blocking_class` get the wall bit in this direction's file.
    """
    data = source.read_bytes()
    if len(data) < 64:
        raise ValueError("Truncated RCL2")
    h = struct.unpack_from("<16I", data)
    if h[:4] != (0x52434C32, 2, 0, 64) or h[5] != 64 or h[8] != 0:
        raise ValueError("Unexpected RCL2 header")
    vertex_count, vertex_offset = h[4:6]
    triangle_count, triangle_offset = h[6:8]
    cell_count, cell_offset = h[10:12]
    if not (0 < vertex_count <= 32767 and 0 < triangle_count <= 32767 and
            vertex_offset + vertex_count * 32 == triangle_offset and
            triangle_offset + triangle_count * 32 == cell_offset and
            cell_offset + cell_count * 56 == h[13] and
            h[12] == triangle_count and h[14] == vertex_count and
            h[13] + triangle_count * 128 == h[15] and
            h[15] + vertex_count * 4 == len(data)):
        raise ValueError("Unexpected RCL2 section layout")
    vertices = data[vertex_offset:triangle_offset]
    cells = data[cell_offset:h[13]]
    triangles = bytearray()
    surface_counts: dict[int, int] = {}
    for i in range(triangle_count):
        row = struct.unpack_from("<8I", data, triangle_offset + i * 32)
        if any(v >= vertex_count for v in row[:3]) or any(
                v != 0xFFFFFFFF and v >= triangle_count for v in row[3:6]):
            raise ValueError(f"Invalid RCL2 triangle {i}")
        if row[7] or row[6] & 0xFFFF:
            raise ValueError(f"Unsupported RCL2 triangle fields at {i}")
        flags = row[6] >> 16
        surface_counts[flags] = surface_counts.get(flags, 0) + 1
        if flags < 0x8000 and flags & 0xF0 == blocking_class:
            flags |= 0x8000
        links = [v if v != 0xFFFFFFFF else -1 for v in row[3:6]]
        triangles += struct.pack("<8h", *row[:3], *links, 0,
                                 flags if flags < 32768 else flags - 65536)
    # Coarse cells retain their 56-byte RCL1 structure and triangle seeds.
    for i in range(cell_count):
        seed = struct.unpack_from("<h", cells, i * 56 + 52)[0]
        if seed < 0 or seed >= triangle_count:
            raise ValueError(f"Invalid RCL2 coarse-cell seed {i}")
    cells, retired, added = repair_coarse_cells(
        [struct.unpack_from("<3f", vertices, i * 32) for i in range(vertex_count)],
        [struct.unpack_from("<8h", triangles, i * 16) for i in range(triangle_count)],
        cells, center)
    cell_count = len(cells) // 56
    material_offset = 48
    vertex_out = material_offset + 36
    triangle_out = vertex_out + len(vertices)
    cell_out = triangle_out + len(triangles)
    header = struct.pack("<12I", 0x52434C31, 1, 1, material_offset,
                         vertex_count, vertex_out, triangle_count, triangle_out,
                         0, cell_out, cell_count, cell_out)
    destination.write_bytes(header + bytes(36) + vertices + triangles + cells)
    return {"vertices": vertex_count, "triangles": triangle_count,
            "coarseCells": cell_count, "surfaceFlags": surface_counts,
            "sourceCoarseCells": h[10], "retiredCoarseCells": retired,
            "triangleCoarseCells": added}


def cell_distance(cell: tuple[float, ...], point: tuple[float, ...]) -> float:
    """D3 coarse-cell test: plane distance, or -1 outside the XZ triangle."""
    x, y, z = point
    distance = cell[0] * x + cell[1] * y + cell[2] * z + cell[3]
    if distance < 0:
        return -1
    previous = (cell[10], cell[12])
    for at in (4, 7, 10):
        next_point = (cell[at], cell[at + 2])
        cross = ((next_point[1] - previous[1]) * (x - previous[0]) -
                 (next_point[0] - previous[0]) * (z - previous[1]))
        if cross < 0:
            return -1
        previous = next_point
    return distance


def walk_triangles(vertices: list[tuple[float, float, float]],
                   triangles: list[tuple[int, ...]], start: int,
                   x: float, z: float) -> tuple[int, int]:
    """D3's linked-triangle walk: (triangle or -1, triangles visited, max 100)."""
    triangle_index, previous_triangle = start, -2
    for visited in range(1, 101):
        triangle = triangles[triangle_index]
        previous_vertex = vertices[triangle[2]]
        outside = -1
        for edge in range(3):
            next_vertex = vertices[triangle[edge]]
            if triangle[3 + edge] != previous_triangle:
                cross = ((next_vertex[2] - previous_vertex[2]) *
                         (x - previous_vertex[0]) -
                         (next_vertex[0] - previous_vertex[0]) *
                         (z - previous_vertex[2]))
                if cross < 0:
                    outside = edge
                    break
            previous_vertex = next_vertex
        if outside < 0:
            return triangle_index, visited
        previous_triangle = triangle_index
        triangle_index = triangle[3 + outside]
        if triangle_index < 0:
            return -1, visited
    return -1, 100


def repair_coarse_cells(vertices: list[tuple[float, float, float]],
                        triangles: list[tuple[int, ...]],
                        cells: bytes,
                        center: list[tuple[float, float, float]]) -> tuple[bytes, list[int], int]:
    """Replace coarse cells whose seed cannot reach the mesh beneath them.

    D3 enters a cell through its seed whenever the cell changes, and its hinted
    search takes the first containing cell in index order from the previous
    one, so every seed must reach all locatable triangles under its cell or the
    result depends on that order. Many IDZero cells overlap neighbouring road
    sections or concave wall strips; a walk from their seed stops at a border
    edge. Those cells are retired (their plane never contains a point) and each
    triangle touching them gets its own cell under the retired cell's plane,
    seeded with itself, or for a wall with a linked road triangle (walls not
    linked to the road get none). The hinted search scans outward in cell
    index, and neighbouring road legs overlap here, so added cells follow the
    route at their seed's nearest centre point, finding the car's own section
    first. Other
    source cells are unchanged. Returns (cells, retired, added count).
    """
    samples: list[tuple[float, float, float]] = []
    owners: list[int] = []
    for index, triangle in enumerate(triangles):
        corners = [vertices[v] for v in triangle[:3]]
        centroid = [sum(c[k] for c in corners) / 3 for k in range(3)]
        # Near-vertical walls can have no XZ interior; D3 cannot locate them.
        if walk_triangles(vertices, triangles, index, centroid[0], centroid[2])[0] != index:
            continue
        for weights in ((1 / 3, 1 / 3, 1 / 3), (.8, .1, .1), (.1, .8, .1), (.1, .1, .8),
                        (.45, .45, .1), (.1, .45, .45), (.45, .1, .45)):
            point = [sum(w * c[k] for w, c in zip(weights, corners)) for k in range(3)]
            samples.append((point[0], point[1] + 1, point[2]))
            owners.append(index)
    size = 16.0
    buckets: dict[tuple[int, int], list[int]] = {}
    for i, (x, _, z) in enumerate(samples):
        buckets.setdefault((math.floor(x / size), math.floor(z / size)), []).append(i)

    def inside(cell: tuple[float, ...]) -> list[int]:
        xs, zs = (cell[4], cell[7], cell[10]), (cell[6], cell[9], cell[12])
        return [i for bx in range(math.floor(min(xs) / size), math.floor(max(xs) / size) + 1)
                for bz in range(math.floor(min(zs) / size), math.floor(max(zs) / size) + 1)
                for i in buckets.get((bx, bz), ()) if cell_distance(cell, samples[i]) >= 0]

    def strands(cell: tuple[float, ...]) -> bool:
        """True if the seed's walk misses a triangle under the cell. Road must
        be found at its own level; walls, which the original sweep locates as
        its destination, may resolve to any triangle."""
        for i in inside(cell):
            x, y, z = samples[i]
            found = walk_triangles(vertices, triangles, cell[13], x, z)[0]
            if found < 0 or triangles[owners[i]][7] >= 0 and abs(
                    sum(vertices[v][1] for v in triangles[found][:3]) / 3 - (y - 1)) > 3:
                return True
        return False

    rows = [struct.unpack_from("<13fh", cells, c * 56) for c in range(len(cells) // 56)]
    retired = [c for c, cell in enumerate(rows) if strands(cell)]
    def overlaps(a: list[tuple[float, float]], b: list[tuple[float, float]]) -> bool:
        """Separating-axis test for two XZ triangles."""
        for shape in (a, b):
            for k in range(3):
                (x0, z0), (x1, z1) = shape[k], shape[(k + 1) % 3]
                axis = (z1 - z0, x0 - x1)
                pa = [axis[0] * x + axis[1] * z for x, z in a]
                pb = [axis[0] * x + axis[1] * z for x, z in b]
                if max(pa) < min(pb) or max(pb) < min(pa):
                    return False
        return True

    # Every locatable triangle touching a retired cell, not only those with a
    # sample inside it, or its uncovered part would be left without a cell.
    locatable = sorted(set(owners))
    planes: dict[int, tuple[float, ...]] = {}
    for c in retired:
        cell = [(rows[c][4], rows[c][6]), (rows[c][7], rows[c][9]), (rows[c][10], rows[c][12])]
        for index in locatable:
            if index not in planes and overlaps(
                    cell, [(vertices[v][0], vertices[v][2]) for v in triangles[index][:3]]):
                planes[index] = rows[c][:4]
    out = bytearray(cells)
    for c in retired:
        struct.pack_into("<4f", out, c * 56, 0, 0, 0, -1)
        rows[c] = (0, 0, 0, -1, *rows[c][4:])
    def road_seed(index: int) -> int | None:
        """Nearest linked driveable triangle whose walk reaches this wall.

        The original sweep walks from the destination cell's seed back to the
        previous point on the road, which a seed inside a wall strip cannot
        reach, so wall cells enter from the road side. None for a wall strip
        not linked to the road; the linked wall it overlaps serves its area."""
        targets = [i for i in by_owner.get(index, ())]
        frontier, seen = [index], {index}
        for _ in range(8):
            following = []
            for t in frontier:
                for n in triangles[t][3:6]:
                    if n >= 0 and n not in seen:
                        seen.add(n)
                        following.append(n)
            for n in following:
                if triangles[n][7] >= 0 and all(walk_triangles(
                        vertices, triangles, n, samples[i][0], samples[i][2])[0] >= 0 for i in targets):
                    return n
            frontier = following
        return None

    by_owner: dict[int, list[int]] = {}
    for i, index in enumerate(owners):
        by_owner.setdefault(index, []).append(i)
    route_buckets: dict[tuple[int, int], list[int]] = {}
    for k, (x, _, z) in enumerate(center):
        route_buckets.setdefault((math.floor(x / 32), math.floor(z / 32)), []).append(k)

    def route_index(index: int) -> int:
        x = sum(vertices[v][0] for v in triangles[index][:3]) / 3
        z = sum(vertices[v][2] for v in triangles[index][:3]) / 3
        for reach in range(1, 8):
            near = [k for bx in range(math.floor(x / 32) - reach, math.floor(x / 32) + reach + 1)
                    for bz in range(math.floor(z / 32) - reach, math.floor(z / 32) + reach + 1)
                    for k in route_buckets.get((bx, bz), ())]
            if near:
                return min(near, key=lambda k: (center[k][0] - x) ** 2 + (center[k][2] - z) ** 2)
        return len(center)

    seeded = [(index, plane, index if triangles[index][7] >= 0 else road_seed(index))
              for index, plane in planes.items()]
    seeded = sorted(((route_index(seed), index, plane, seed) for index, plane, seed in seeded
                     if seed is not None), key=lambda item: item[:2])
    added = len(seeded)
    for _, index, plane, seed in seeded:
        corners = [vertices[v] for v in triangles[index][:3]]
        row = (*plane, *corners[0], *corners[1], *corners[2], seed)
        out += struct.pack("<13fh", *row) + bytes(2)
        rows.append(row)
    stranded = [c for c, cell in enumerate(rows) if strands(cell)]
    if stranded:
        raise ValueError(f"Coarse cells {stranded} still cannot reach their mesh")
    return bytes(out), retired, added


def verify_road_contact(collision: Path,
                        center: list[tuple[float, float, float]]) -> int:
    """Exercise RCL1 coarse lookup and linked triangles across the timed road."""
    data = collision.read_bytes()
    h = struct.unpack_from("<12I", data)
    vertices = [struct.unpack_from("<3f", data, h[5] + i * 32)
                for i in range(h[4])]
    triangles = [struct.unpack_from("<8h", data, h[7] + i * 16)
                 for i in range(h[6])]
    cells = [struct.unpack_from("<13fh", data, h[11] + i * 56)
             for i in range(h[10])]

    checked = 0
    for index in range(CHECKPOINTS[0], CHECKPOINTS[-1] + 1, 8):
        x, y, z = center[index]
        sample = (x, y + 1, z)
        candidates = [(cell_distance(cell, sample), cell[13]) for cell in cells]
        candidates = [(distance, seed) for distance, seed in candidates if distance >= 0]
        if not candidates:
            raise ValueError(f"No coarse collision cell at Gunsai path {index}")
        found, visited = walk_triangles(vertices, triangles, min(candidates)[1], x, z)
        if found < 0:
            raise ValueError(f"Road path escapes collision at {index}" if visited < 100
                             else f"Collision link loop at Gunsai path {index}")
        if abs(sum(vertices[v][1] for v in triangles[found][:3]) / 3 - y) > 8:
            raise ValueError(f"Collision height differs at Gunsai path {index}")
        checked += 1
    return checked


def export(source: Path, output: Path) -> None:
    course = source / "data" / "COURSE" / SOURCE
    route = course / "path"
    if not route.is_dir():
        raise FileNotFoundError(route)
    output.mkdir(parents=True, exist_ok=True)
    paths = [route / f"gunsai_path_road_{side}.pa4" for side in ("c", "l", "r")]
    roads = [path_points(path) for path in paths]
    count = len(roads[0])
    if any(len(road) != count for road in roads) or CHECKPOINTS[-1] >= count - 1:
        raise ValueError("Gunsai path and checkpoint counts differ")
    attached = [attach_start(road) for road in roads]
    source_edges = roads[1:]
    aligned = [align_edge(roads[0], edge) for edge in source_edges]
    roads = [roads[0], aligned[0][0], aligned[1][0]]
    widest = check_cross_sections(*roads)
    for i, (center, left, right) in enumerate(zip(*roads)):
        width = math.dist(left, right)
        if not 0.05 < width < 200 or math.dist(center, left) > width * 2:
            raise ValueError(f"Invalid Gunsai road width at {i}")
    for points, suffix in zip(roads, ("", "_l", "_r")):
        write_path(output / f"gunsai_path{suffix}.bin", points)
    (output / "road.bin").write_bytes(b"HKR1" + struct.pack("<I", count) + b"".join(
        struct.pack("<3f", *point) for road in roads for point in road))
    # Initial allowances are D3 adaptation values, to be tuned with driving.
    (output / "race.bin").write_bytes(b"HKD3" + struct.pack("<9i", *CHECKPOINTS,
                                                               120, 70, 70, 70))
    lamps = placements(route / 'gunsai_path_light.pa8')
    (output / "lamps.bin").write_bytes(b"HKL1" + struct.pack("<I", len(lamps)) +
                                      b''.join(struct.pack('<3f', *row[:3]) for row in lamps))
    # Reserve the next imported slot; the runtime catalog must add it before
    # this pack can be loaded, preventing a partial pack defaulting to Hakone.
    (output / "course.id").write_text("16\n")
    collision = course / "collision" / "gunsai_collision.bin"
    collision_report = {}
    for reverse, name in ((False, "gunsai.rcl"), (True, "gunsai-reverse.rcl")):
        variant = convert_collision(collision, output / name, BLOCKING_CLASS[reverse], roads[0])
        variant["blockingClass"] = BLOCKING_CLASS[reverse]
        variant["roadSamplesChecked"] = verify_road_contact(output / name, roads[0])
        collision_report[name] = variant
    report = {
        "source": "Initial D Arcade Stage Zero 2.20 / Gunsai",
        "pathPoints": count,
        "checkpoints": list(CHECKPOINTS),
        "detachedStartReplaced": attached,
        "edgeAlignmentFallbacks": [fallbacks for _, fallbacks in aligned],
        "widestCrossSection": round(widest, 3),
        "collision": collision_report,
        "sourceSha256": {str(path.relative_to(source)).replace("\\", "/"):
                         hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in (*paths, collision)},
    }
    (output / "driving-import.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Gunsai: {count} path points, {collision_report['gunsai.rcl']['triangles']} collision triangles")
    for condition, prefix in [('day_dry', 'day_dry'), ('night_dry', 'ngt_dry')]:
        destination = output if condition == 'day_dry' else output / condition
        scene = Scene(destination)
        scene.archive(course/'efo'/f'gunsai_{prefix}_crs_a.efo')
        scene.archive(course/'efo'/f'gunsai_{prefix}_sky.efo', 'sky')
        trees = scene.archive(course/'efo'/f'gunsai_{prefix}_tree.efo', 'tree')
        scene.trees(route/'gunsai_path_tree.pa8', trees)
        scene.save(course/'env'/f'gunsai_{prefix}_shader_config.ybo', count)
        if destination != output:
            shutil.copyfile(output/'road.bin', destination/'road.bin')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True,
                        help="IDZero package root containing data/COURSE")
    parser.add_argument("--output", type=Path, required=True,
                        help="New Gunsai pack directory")
    arguments = parser.parse_args()
    export(arguments.source, arguments.output)
