"""Read-only HUD interpolation regression against the previous local player DLL.

Uses isolated saves and actual native race input; no GPU, Steam or audio device.
Usage: python Tools/check_hud_presentation.py <new evidence directory>
"""
import ctypes as C
import json
import shutil
import sys
from pathlib import Path

from check_native_multiplayer import Input, Status

ROOT = Path(__file__).resolve().parents[1]
U32, U64, F32 = C.c_uint32, C.c_uint64, C.c_float


class Timing(C.Structure):
    _fields_ = [('size', U32), ('version', U32), ('tick', U64), ('alpha', F32), ('flags', U32)]


class Ornament(C.Structure):
    _fields_ = [('size', U32), ('version', U32), ('tick', U64), ('flags', U32), ('car', U32)] + [(name, F32) for name in ('x', 'y', 'z', 'yaw')]


class Hud(C.Structure):
    _fields_ = [('size', U32), ('version', U32), ('flags', U32), ('gear', C.c_int32)] + [(name, F32) for name in ('speed', 'rpm', 'limit', 'throttle', 'brake', 'drift')]


checks = 0


def check(ok, message):
    global checks
    checks += 1
    if not ok:
        raise AssertionError(message)


class Game:
    def __init__(self, root, name, source):
        self.root = root / name
        self.root.mkdir()
        library = self.root / (name + '.dll')
        shutil.copy2(source, library)
        self.dll = C.CDLL(str(library))
        self.dll.Idas3SceneInitialize.argtypes = [C.c_char_p, C.c_char_p, C.c_int, C.c_int]
        check(self.dll.Idas3SceneInitialize(str(ROOT / 'Native').encode(), str(self.root / 'saves').encode(), 640, 480) == 1, self.error())

    def error(self):
        buffer = C.create_string_buffer(2048)
        self.dll.Idas3UnityCopyError(buffer, len(buffer))
        return buffer.value.decode('utf8', 'replace')

    def read(self, method, kind):
        result = kind(C.sizeof(kind))
        check(getattr(self.dll, method)(C.byref(result)) == 1, method + ': ' + self.error())
        return result

    def step(self, dt=1 / 120, key=None):
        value = Input(C.sizeof(Input), 1, dt)
        value.keys[87 // 32] |= 1 << (87 % 32)
        if key:
            value.keys[key // 32] |= 1 << (key % 32)
        check(self.dll.Idas3SceneStep(C.byref(value)) == 1, self.error())


def main():
    output = Path(sys.argv[1]).resolve()
    output.mkdir(parents=True, exist_ok=False)
    check([C.sizeof(value) for value in (Timing, Ornament, Hud)] == [24, 40, 40], 'Presentation ABI sizes')
    current = Game(output, 'current', ROOT / 'Assets/Plugins/x86_64/Idas3Unity.dll')
    reference = Game(output, 'reference', ROOT / 'Builds/Current/InitialDUnity_Data/Plugins/x86_64/Idas3Unity.dll')
    seen = {}
    intermediate = same_tick_motion = 0
    last = None
    trace = []
    try:
        for frame in range(1800):
            key = 0x74 if frame == 5 else None  # Existing native Time Attack shortcut.
            for game in (current, reference):
                game.step(key=key)
            state = current.read('Idas3UnityGetStatus', Status)
            prior = reference.read('Idas3UnityGetStatus', Status)
            check((state.ticks, state.flags, state.racePhase, state.speed, state.rpm) ==
                  (prior.ticks, prior.flags, prior.racePhase, prior.speed, prior.rpm), 'HUD changed source simulation')
            timing = current.read('Idas3SceneGetPresentationTiming', Timing)
            ornament = current.read('Idas3SceneGetOrnamentTelemetry', Ornament)
            hud = current.read('Idas3SceneGetHudTelemetry', Hud)
            old_hud = reference.read('Idas3SceneGetHudTelemetry', Hud)
            check(timing.tick == ornament.tick and timing.flags == ornament.flags, 'Cosmetic timing disagrees with pose owner')
            check(0 <= timing.alpha <= 1, 'Invalid render interpolation fraction')
            check((hud.gear, hud.flags, hud.limit, hud.throttle, hud.brake) ==
                  (old_hud.gear, old_hud.flags, old_hud.limit, old_hud.throttle, old_hud.brake), 'Discrete HUD signals changed')
            if timing.flags == 1:
                seen[timing.tick] = (state.rpm, state.speed)
                if timing.tick - 1 in seen:
                    rpm, speed = seen[timing.tick - 1]
                    expected_rpm = rpm + (state.rpm - rpm) * timing.alpha
                    expected_speed = (speed + (state.speed - speed) * timing.alpha) * 3.6
                    check(abs(hud.rpm - expected_rpm) < .01, 'Live RPM missed its fixed-step interpolation interval')
                    check(abs(hud.speed - expected_speed) < .0001, 'Live speed missed its fixed-step interpolation interval')
                    intermediate += .1 < timing.alpha < .9 and abs(state.rpm - rpm) > .01
                if last and timing.tick == last[0] and abs(hud.rpm - last[1]) > .01:
                    same_tick_motion += 1
                last = timing.tick, hud.rpm
            else:
                check(timing.alpha == 1, 'Inactive/frozen presentation retained an interpolation interval')
            if frame % 30 == 0:
                trace.append(dict(frame=frame, tick=timing.tick, alpha=timing.alpha, flags=timing.flags, sourceRpm=state.rpm, displayRpm=hud.rpm))
        check(intermediate > 60 and same_tick_motion > 60, 'Needle did not move between 60 Hz solver ticks')
        check(current.dll.Idas3SceneSetPaused(1) == 1, current.error())
        frozen = current.read('Idas3SceneGetHudTelemetry', Hud)
        for _ in range(12):
            current.step()
            hud = current.read('Idas3SceneGetHudTelemetry', Hud)
            timing = current.read('Idas3SceneGetPresentationTiming', Timing)
            check(hud.rpm == frozen.rpm and hud.speed == frozen.speed and timing.flags == 3 and timing.alpha == 1, 'Pause moved meters or cosmetic clock')
        check(current.dll.Idas3SceneSetPaused(0) == 1, current.error())
        check(current.read('Idas3SceneGetHudTelemetry', Hud).rpm == frozen.rpm, 'Resume rewound meter before a new solver tick')
        current.step(1 / 240)
        check(current.read('Idas3SceneGetHudTelemetry', Hud).rpm == frozen.rpm, 'Resume replayed stale RPM interval')
        report = dict(result='PASS', checks=checks, comparedFrames=1800, rendererFps=120,
                      intermediateNeedleFrames=intermediate, sameTickNeedleMotion=same_tick_motion,
                      actualPhysicsUnchanged=True, trace=trace)
        (output / 'report.json').write_text(json.dumps(report, indent=2))
        print(json.dumps({key: value for key, value in report.items() if key != 'trace'}))
    finally:
        current.dll.Idas3SceneShutdown()
        reference.dll.Idas3SceneShutdown()


if __name__ == '__main__':
    main()
