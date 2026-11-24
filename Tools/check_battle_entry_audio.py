"""Check the linked cabinet menus using the real mixer without opening an audio device."""
import argparse
import ctypes as C
from pathlib import Path
import tempfile

from check_native_multiplayer import Input


def capture(dll, assets, saves, page, music, effects, navigate=False):
    saves.mkdir()
    assert dll.Idas3SceneInitialize(str(assets).encode(), str(saves).encode(), 640, 480) == 1
    try:
        dll.Idas3UnitySetAudioRunning(1)
        options = (C.c_uint32 * 10)(40)
        assert dll.Idas3SceneGetOptions(options) == 1
        gains = (C.c_float * 4).from_buffer(options, 8)
        gains[:] = (1, music, 0, effects)
        assert dll.Idas3SceneApplyOptions(options) == 1
        assert dll.Idas3SceneChallenger(2) == 1
        state = (C.c_int * 370)()
        state[0], state[2], state[363] = page, 30, -1
        samples = (C.c_float * 1470)()
        blocks = []
        for frame in range(420):
            state[3] = frame
            if navigate and frame == 360:
                if page == 10:
                    state[1] = 1  # Waiting driver's next opponent-info page.
                else:
                    state[6] = 1  # Picker's next course.
            assert dll.Idas3SceneBattleEntry(state, len(state), b'DRIVER', b'', b'', b'') == 1
            inp = Input(C.sizeof(Input), 1, 1/60)
            inp.width, inp.height = 640, 480
            assert dll.Idas3SceneStep(C.byref(inp)) == 1
            n = dll.Idas3UnityReadAudio(samples, 735)
            assert 0 <= n <= 735
            blocks.append(max(map(abs, samples[:2*n]), default=0))
        return blocks
    finally:
        dll.Idas3UnitySetAudioRunning(0)
        dll.Idas3SceneShutdown()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dll', type=Path)
    parser.add_argument('assets', type=Path, help='StreamingAssets/IDAS3 directory')
    args = parser.parse_args()
    dll = C.CDLL(str(args.dll.resolve()))
    dll.Idas3SceneInitialize.argtypes = [C.c_char_p, C.c_char_p, C.c_int, C.c_int]
    with tempfile.TemporaryDirectory() as tmp:
        for page, role in ((6, 'picker'), (10, 'waiting')):
            def run(label, music, effects, navigate=False):
                return capture(dll, args.assets.resolve(), Path(tmp)/(role+'-'+label),
                               page, music, effects, navigate)
            music = run('music', 1, 0)
            fx = run('effects', 0, 1, True)
            silent = run('muted', 0, 0, True)
            assert sum(p > .0001 for p in music[240:360]) > 110, role+' selection score'
            assert max(fx[:90]) > .01, role+' challenge cue'
            assert max(fx[300:355]) < .001, role+' repeated challenge cue'
            assert max(fx[360:]) > .01, role+' navigation sound'
            assert max(silent) == 0, role+' mute'
            print(role, 'passed challenge, sustained music, navigation and mute')


if __name__ == '__main__':
    main()
