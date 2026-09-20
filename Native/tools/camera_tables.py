"""Parse the original per-course camera shot tables.

Container (verified against all 19 files on the disc):

    u32 count
    count times:
        u32 size                # payload bytes, always a multiple of 4
        u8  payload[size]       # payload[0..4) is the record kind

Every record shares a prefix laid out like the first eight words of
`OriginalStartCameraShot`. What is actually proven over all 819 records:

    w0  kind
    w1  smoothJoint     always 0 or 1 (1 in only 6 records)
    w2  isShake         always 0 or 1 (1 in 104 records)
    w3..w5  world position -- for every kind EXCEPT 0 and 6, whose values here
        never exceed 12 and are therefore parameters, not coordinates
    w6  angleRadians    always in field-of-view range; pi/3 in 360 of 819
    w7  non-negative in kinds 2,5,6,9,10,12,13 (0.2..28, reads as a hold in
        seconds); goes negative in kinds 0,1,3,4,8,11, so it means something
        else there. This split is an observation about the data, not a proven
        record taxonomy -- kind 6 sits on the non-negative side yet carries no
        position.

Kind 5 is exactly the 18-word `OriginalStartCameraShot`; kind 13 is that plus
two words. The remaining kinds are not decoded past the common prefix.

The game picks a file with `table[course*2 + direction]` (table at 0x0c33b920,
selector at 0x0c192280): course 8 reuses Akina's files, and course 9 with
direction 0 is the special case that returns the ending camera.
"""
from pathlib import Path
import struct, sys

BINARY = Path(r'C:/Users/Developer/Documents/Codex/Initial D Arcade Stage 3 Recompiled'
              r' - Crash Safe Test/game files/driveA/HOSTFS/binary')

# table[course*2 + direction] as read out of the game image
FILES = ['y_camdata_00', 'y_camdata_01', 'y_camdata_10', 'y_camdata_11',
         'y_camdata_20', 'y_camdata_21', 'y_camdata_30', 'y_camdata_31',
         'y_camdata_40', 'y_camdata_41', 'y_camdata_50', 'y_camdata_51',
         'o_camdata_60', 'o_camdata_61', 'o_camdata_70', 'o_camdata_71',
         'y_camdata_30', 'y_camdata_31']          # course 8 reuses Akina
ENDING = 'o_ending_camera_00'                     # course 9, direction 0

# Neither of these is reachable through the table; o_camadv has its own
# reference in the image, o_camdata_30 has none at all.
UNREFERENCED = ['o_camadv', 'o_camdata_30']

# Kinds whose w7 is never negative and reads as a hold in seconds.
HOLD_KINDS = {2, 5, 6, 9, 10, 12, 13}
# Kinds whose w3..w5 are not world coordinates.
POSITIONLESS_KINDS = {0, 6}


def select(course, direction):
    """The original selector at 0x0c192280."""
    if course == 9 and direction == 0:
        return ENDING
    if course > 8:
        course = 0
    if direction > 1:
        direction = 0
    return FILES[course * 2 + direction]


def records(name, root=BINARY):
    """Yield (index, kind, payload_words) for one table."""
    raw = (Path(root) / (name + '.bin.nz')).read_bytes()
    count = struct.unpack_from('<I', raw, 0)[0]
    at = 4
    for i in range(count):
        size = struct.unpack_from('<I', raw, at)[0]
        if size < 8 or size % 4 or at + 4 + size > len(raw):
            raise ValueError(f'{name}: bad record {i} size {size} at 0x{at:x}')
        yield i, struct.unpack_from('<I', raw, at + 4)[0], \
            struct.unpack_from('<%dI' % (size // 4), raw, at + 4)
        at += 4 + size


def shot(words):
    """Decode the common prefix. Fields past w7 are kind-specific."""
    f = [struct.unpack('<f', struct.pack('<I', w))[0] for w in words]
    return dict(kind=words[0], smoothJoint=words[1], isShake=words[2],
                position=(f[3], f[4], f[5]), angleRadians=f[6], field7=f[7])


def main():
    names = sys.argv[1:] or sorted(set(FILES)) + [ENDING] + UNREFERENCED
    for name in names:
        rs = list(records(name))
        holds = sum(1 for _, k, _ in rs if k in HOLD_KINDS)
        print(f'{name:22s} {len(rs):3d} records, {holds:3d} with a hold time')
        for i, kind, words in rs:
            s = shot(words)
            hold = f'{s["field7"]:7.2f}s' if kind in HOLD_KINDS else f'{s["field7"]:8.3f}'
            where = ('     (not a position)      ' if kind in POSITIONLESS_KINDS
                     else f'({s["position"][0]:9.2f},{s["position"][1]:8.2f},{s["position"][2]:10.2f})')
            print(f'   {i:3d} kind {kind:2d}  {where}'
                  f'  fov {s["angleRadians"]:.4f}  {hold}'
                  f'{"  shake" if s["isShake"] else ""}'
                  f'{"  smooth" if s["smoothJoint"] else ""}')


if __name__ == '__main__':
    main()
