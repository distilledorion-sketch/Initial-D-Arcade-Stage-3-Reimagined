"""Software-render a converted HKN2 imported-course scene from a road point.

Menu artwork source only (thumbnail-preview.png); this is not the game's
renderer: textures and baked vertex colours, no trees, shadows or lighting.

    python Tools/Render-ImportedScenePreview.py RuntimeAssets/ODAWARA out.png --back 14 --height 1.8
"""
import argparse
import json
import math
import struct
from pathlib import Path

import numpy as np
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("pack", type=Path, help="Folder holding scene.json, scene.bin and road.bin")
parser.add_argument("out")
parser.add_argument("--index", type=int, default=0, help="Road point the camera stands at")
parser.add_argument("--back", type=float, default=0, help="Distance behind the point")
parser.add_argument("--height", type=float, default=1.8)
parser.add_argument("--side", type=float, default=0, help="Offset to the camera's right")
parser.add_argument("--yaw", type=float, default=0, help="Degrees to turn from the road direction")
parser.add_argument("--reverse", action="store_true", help="Look against the path order")
parser.add_argument("--hide", default=None, help="Skip materials with this name prefix")
args = parser.parse_args()
pack, out, index, back, height, side = args.pack, args.out, args.index, args.back, args.height, args.side
yaw_offset, reverse = args.yaw, args.reverse
W, H, SS, FOV, FAR = 794, 436, 2, 62.0, 2500.0
w, h = W * SS, H * SS

manifest = json.loads((pack / 'scene.json').read_text())
road = (pack / 'road.bin').read_bytes(); count = struct.unpack_from('<I', road, 4)[0]
centre = np.frombuffer(road, '<f4', count * 3, 8).reshape(-1, 3).astype(np.float64)
p0 = centre[index]; direction = centre[(index + 3) % (count - 1)] - centre[index - 3]
if reverse: direction = -direction
direction[1] = 0; direction /= np.linalg.norm(direction)
c, s = math.cos(math.radians(yaw_offset)), math.sin(math.radians(yaw_offset))
direction = np.array([direction[0] * c - direction[2] * s, 0, direction[0] * s + direction[2] * c])
up = np.array([0., 1., 0.]); right = np.cross(direction, up)
eye = p0 - direction * back + up * height + right * side
focal = (w / 2) / math.tan(math.radians(FOV) / 2)

textures = {}
def texture(name):
    if name not in textures:
        try:
            image = Image.open(pack / name).convert('RGBA')
        except Exception as error:
            print('texture', name, error); image = Image.new('RGBA', (4, 4), (160, 160, 160, 255))
        levels = [np.asarray(image, np.float32) / 255]
        while min(levels[-1].shape[:2]) > 2 and len(levels) < 9:
            a = levels[-1]; a = a[:a.shape[0] // 2 * 2, :a.shape[1] // 2 * 2]
            levels.append((a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2]) / 4)
        textures[name] = levels
    return textures[name]

color = np.zeros((h, w, 3), np.float32); depth = np.full((h, w), np.inf, np.float32)
data = (pack / 'scene.bin').read_bytes(); at = 8; shapes = struct.unpack_from('<I', data, 4)[0]
vertex = np.dtype([('p', '<f4', 3), ('n', '<f4', 3), ('uv', '<f4', 2), ('uv2', '<f4', 2), ('c', 'u1', 4)])
NEAR = 0.3

def draw(P, UV, C, levels, cutoff, sky):
    """P: (3,3) camera space x right, y up, z forward."""
    inside = P[:, 2] > NEAR
    if not inside.any(): return
    polygon = [(P[i], UV[i], C[i]) for i in range(3)]
    if not inside.all():
        clipped = []
        for i in range(3):
            a, b = polygon[i], polygon[(i + 1) % 3]
            if a[0][2] > NEAR: clipped.append(a)
            if (a[0][2] > NEAR) != (b[0][2] > NEAR):
                t = (NEAR - a[0][2]) / (b[0][2] - a[0][2])
                clipped.append(tuple(a[k] + t * (b[k] - a[k]) for k in range(3)))
        polygon = clipped
    for k in range(1, len(polygon) - 1):
        tri = (polygon[0], polygon[k], polygon[k + 1])
        z = np.array([v[0][2] for v in tri]); x = w / 2 + focal * np.array([v[0][0] for v in tri]) / z
        y = h / 2 - focal * np.array([v[0][1] for v in tri]) / z
        x0, x1 = max(int(math.floor(x.min())), 0), min(int(math.ceil(x.max())), w - 1)
        y0, y1 = max(int(math.floor(y.min())), 0), min(int(math.ceil(y.max())), h - 1)
        if x0 > x1 or y0 > y1: continue
        area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
        if abs(area) < 1e-6: continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        b0 = ((x[1] - gx) * (y[2] - gy) - (x[2] - gx) * (y[1] - gy)) / area
        b1 = ((x[2] - gx) * (y[0] - gy) - (x[0] - gx) * (y[2] - gy)) / area
        b2 = 1 - b0 - b1
        mask = (b0 >= 0) & (b1 >= 0) & (b2 >= 0)
        if not mask.any(): continue
        iw = b0 / z[0] + b1 / z[1] + b2 / z[2]
        zz = 1 / iw
        region = depth[y0:y1 + 1, x0:x1 + 1]
        if not sky: mask &= zz < region
        if not mask.any(): continue
        uv = sum((b / z[i])[..., None] * tri[i][1] for i, b in enumerate((b0, b1, b2))) * zz[..., None]
        col = sum((b / z[i])[..., None] * tri[i][2] for i, b in enumerate((b0, b1, b2))) * zz[..., None]
        if levels is None:
            texel = np.ones(uv.shape[:2] + (4,), np.float32)
        else:
            base = levels[0].shape
            duv = max(np.ptp([v[1][0] for v in tri]) * base[1], np.ptp([v[1][1] for v in tri]) * base[0], 1e-6)
            pixels = max(x.max() - x.min(), y.max() - y.min(), 1e-6)
            level = levels[min(len(levels) - 1, max(0, int(round(math.log2(max(duv / pixels, 1))))))]
            th, tw = level.shape[:2]
            texel = level[np.floor(uv[..., 1] * th).astype(np.int64) % th, np.floor(uv[..., 0] * tw).astype(np.int64) % tw]
        alpha = texel[..., 3] * col[..., 3]
        mask &= alpha > (cutoff if cutoff > 0 else 0.02)
        if not mask.any(): continue
        rgb = texel[..., :3] * col[..., :3]
        if not sky:
            fog = np.clip((zz - 150) / 1800, 0, .55)[..., None]
            rgb = rgb * (1 - fog) + np.array([.72, .80, .88], np.float32) * fog
            region[mask] = zz[mask]
        color[y0:y1 + 1, x0:x1 + 1][mask] = rgb[mask]

basis = np.stack([right, up, direction])
queue = []
for shape in range(shapes):
    material, nv, nt = struct.unpack_from('<3I', data, at); matrix = np.array(struct.unpack_from('<12f', data, at + 12)).reshape(3, 4); at += 60
    vertices = np.frombuffer(data, vertex, nv, at); indices = np.frombuffer(data, '<u4', nt, at + nv * 44).reshape(-1, 3); at += nv * 44 + nt * 4
    m = manifest['materials'][material]
    if m['kind'] == 'tree' or m['shadow'] or (len(m['textures']) == 1 and m['textures'][0]['type'] == 6): continue
    if args.hide and m['name'].startswith(args.hide): continue
    queue.append((not m['sky'], shape, m, matrix, vertices, indices))
queue.sort(key=lambda item: item[:2])
drawn = 0
for _, shape, m, matrix, vertices, indices in queue:
    position = vertices['p'].astype(np.float64) @ matrix[:, :3].T + matrix[:, 3]
    if m['sky']: position = position * 2000 + eye
    cam = (position - eye) @ basis.T
    name = next((t['file'] for t in m['textures'] if t['type'] == 1), None)
    levels = texture(name) if name else None
    colors = vertices['c'].astype(np.float32) / 255
    tz = cam[indices][:, :, 2]; tx = cam[indices][:, :, 0]; ty = cam[indices][:, :, 1]
    keep = (tz.max(1) > NEAR) & (tz.min(1) < (1e9 if m['sky'] else FAR))
    keep &= (tx.max(1) > -tz.max(1) * (w / 2 / focal) * 1.05) & (tx.min(1) < tz.max(1) * (w / 2 / focal) * 1.05)
    keep &= (ty.max(1) > -tz.max(1) * (h / 2 / focal) * 1.05) & (ty.min(1) < tz.max(1) * (h / 2 / focal) * 1.05)
    for tri in indices[keep]:
        draw(cam[tri], vertices['uv'][tri].astype(np.float64), colors[tri], levels, m['cutoff'], m['sky']); drawn += 1
print('triangles drawn', drawn, 'eye', eye)
image = Image.fromarray((np.clip(color, 0, 1) * 255).astype(np.uint8)).resize((W, H), Image.LANCZOS)
image.save(out)
