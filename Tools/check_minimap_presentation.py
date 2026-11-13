"""Compare minimap render cadence and unchanged physics with a prior DLL.

Runs real native race input with private saves, no GPU/audio device or network.
Usage: python Tools/check_minimap_presentation.py NEW-DLL REFERENCE-DLL NEW-OUTPUT
"""
import ctypes as C
import json
import sys
from pathlib import Path

from check_hud_presentation import Game
from check_native_multiplayer import Status

U32, F32 = C.c_uint32, C.c_float


class UiFrame(C.Structure):
    _fields_ = [(n, U32) for n in 'size width height draws vertices textures unresolved reserved'.split()] + [('revision', C.c_uint64)]


class Draw(C.Structure):
    _fields_ = [(n, U32) for n in 'first count texture tsp pcw flags'.split()] + [('opacity', F32), ('reserved', U32), ('clip', F32 * 4)]


class Vertex(C.Structure):
    _fields_ = [(n, F32) for n in 'x y u v'.split()] + [('argb', U32), ('offset', U32)]


def map_vertices(game):
    frame = game.read('Idas3UiGetFrame', UiFrame)
    assert frame.unresolved == 0
    draws, vertices = (Draw * frame.draws)(), (Vertex * frame.vertices)()
    assert game.dll.Idas3UiCopyDraws(draws, len(draws)) == len(draws)
    assert game.dll.Idas3UiCopyVertices(vertices, len(vertices)) == len(vertices)
    # First map batch is the moving original navigation field; the frame and
    # fixed centre marker must not make a motion assertion pass accidentally.
    for draw in draws:
        if draw.flags & 8 and (draw.flags >> 8) & 15 == 5:
            return tuple((vertices[i].u, vertices[i].v) for i in range(draw.first, draw.first + draw.count))
    return ()


def main():
    new, old, output = map(Path, sys.argv[1:])
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    current, reference = Game(output, 'current', new.resolve()), Game(output, 'reference', old.resolve())
    previous = None
    changes = [0, 0]
    compared = 0
    try:
        for frame in range(1800):
            for game in (current, reference):
                game.step(key=0x74 if frame == 5 else None)
            a, b = (game.read('Idas3UnityGetStatus', Status) for game in (current, reference))
            assert (a.ticks, a.flags, a.racePhase, a.speed, a.rpm) == (b.ticks, b.flags, b.racePhase, b.speed, b.rpm), 'Map changed physics or race state'
            now = (a.ticks, map_vertices(current), map_vertices(reference))
            if previous and a.speed > 5 and a.ticks == previous[0] and all(now[1:]) and all(previous[1:]):
                compared += 1
                for i in (0, 1):
                    changes[i] += now[i + 1] != previous[i + 1]
            previous = now
        assert compared > 120, 'Insufficient moving frames between solver ticks'
        assert changes[0] > compared * .95, 'Map still waits for simulation ticks'
        assert changes[1] == 0, 'Reference no longer reproduces fixed-tick map motion'
        assert current.dll.Idas3SceneSetPaused(1) == 1
        current.step()
        frozen = map_vertices(current)
        assert frozen
        for _ in range(12):
            current.step()
            assert map_vertices(current) == frozen, 'Map moved during pause'
        report = dict(passed=True, comparedFrames=1800, renderHz=120,
                      movingSubtickFrames=compared, correctedMapChanges=changes[0], referenceMapChanges=changes[1],
                      pauseStable=True, physicsUnchanged=True, audioDeviceOpened=False)
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(report), flush=True)
    finally:
        current.dll.Idas3SceneShutdown()
        reference.dll.Idas3SceneShutdown()


if __name__ == '__main__':
    main()
