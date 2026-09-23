"""Two actual native authority scenes; private saves, no GPU, Steam or real lobby.

python -X utf8 Tests/Multiplayer/test_opponent_headlights.py --output <NEW directory>
Use --dll <old DLL> --expect-baseline-bug to record the unfixed night-lamp bug.
The opt-in Unity LAN smoke additionally covers the production managed transport.
"""
import argparse
import ctypes as C
import hashlib
import importlib.util
import json
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("native_scene", ROOT / "Tools/check_native_multiplayer.py")
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
U32, U64 = C.c_uint32, C.c_uint64


class AuthorityStatus(C.Structure):
    _fields_ = [("size", U32), ("version", U32)] + [
        (name, U64) for name in "frame confirmed verified contactFrames rollbacks replayedFrames".split()
    ] + [("maxDepth", U32), ("stalled", U32), ("maxCorrection", C.c_float),
         ("maxReplayMs", C.c_float), ("winner", C.c_int32), ("reserved", U32),
         ("hostFinishTicks", U64), ("clientFinishTicks", U64)]


class Client(native.Client):
    def __init__(self, output, tag, dll, baseline):
        super().__init__(output, tag, dll)
        self.baseline = baseline
        self.dll.Idas3MultiplayerEnableAuthorityRules.argtypes = [U64, C.c_int, C.c_int, C.c_int]
        if not baseline:
            self.dll.Idas3MultiplayerSetRemoteHeadlights.argtypes = [U64, U32]
        self.packet = (C.c_ubyte * 65536)()

    def begin(self, car, peer_car, slot, night, race):
        config = native.Config(C.sizeof(native.Config), 2, 3, 0, 0, int(night), car, peer_car, slot, 1)
        native.check(self.dll.Idas3MultiplayerStart(C.byref(config)) == 1, self.reason())
        native.check(self.dll.Idas3MultiplayerEnableAuthorityRules(race, 1, 0, 0) == 1, self.reason())

    def remote(self):
        value = native.Snapshot(C.sizeof(native.Snapshot))
        native.check(self.dll.Idas3MultiplayerGetRemoteSnapshot(C.byref(value)) == 1, self.reason())
        return value

    def authority(self):
        value = AuthorityStatus(C.sizeof(AuthorityStatus), 1)
        native.check(self.dll.Idas3MultiplayerAuthorityStatus(C.byref(value)) == 1, self.reason())
        return value

    def send_to(self, peer, lights=True):
        count = self.dll.Idas3MultiplayerAuthorityPacket(self.packet, len(self.packet))
        native.check(40 <= count <= len(self.packet), self.reason())
        native.check(peer.dll.Idas3MultiplayerAuthorityReceive(self.packet, count) == 1, peer.reason())
        if lights and not self.baseline:
            pose = self.snapshot()
            native.check(peer.set_lights(pose.sequence, bool(pose.flags & 16)) == 1, peer.reason())

    def set_lights(self, sequence, enabled):
        return self.dll.Idas3MultiplayerSetRemoteHeadlights(sequence, int(enabled))


def describe(pose):
    return {"car": pose.car, "on": bool(pose.flags & 16), "visible": pose.headlightVisible,
            "phase": pose.headlightPhase, "ticks": pose.raceTicks, "sequence": pose.sequence}


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--dll", type=Path, default=ROOT / "Assets/Plugins/x86_64/Idas3Unity.dll")
    args.add_argument("--output", required=True, type=Path)
    args.add_argument("--expect-baseline-bug", action="store_true")
    args = args.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    started = time.time()
    report = {"schema": "idas3-opponent-headlights-native-v1", "passed": False,
              "baseline": args.expect_baseline_bug, "dll": str(args.dll.resolve()),
              "sha256": hashlib.sha256(args.dll.read_bytes()).hexdigest(), "cases": []}
    clients = []
    try:
        native.check(C.sizeof(AuthorityStatus) == 96, "authority ABI")
        a = Client(output, "host", args.dll, args.expect_baseline_bug)
        clients.append(a)
        b = Client(output, "join", args.dll, args.expect_baseline_bug)
        clients.append(b)

        def tick(count=1, host_key=None, join_key=None, lights=True):
            for _ in range(count):
                a.step(key=host_key)
                b.step(key=join_key)
                a.send_to(b, lights)
                b.send_to(a, lights)

        def state(label, host_on, join_on):
            poses = [a.snapshot(), a.remote(), b.snapshot(), b.remote()]
            expected = [host_on, join_on, join_on, host_on]
            for pose, enabled in zip(poses, expected):
                native.check(bool(pose.flags & 16) == enabled, label + ": lamp flag")
                native.check(bool(pose.headlightVisible) == enabled, label + ": native lamp geometry")
            report["cases"].append({"name": label, "poses": [describe(p) for p in poses]})

        for race_index, night in enumerate([True] if args.expect_baseline_bug else [True, False]):
            a.begin(0, 8, 0, night, 4001 + race_index)
            b.begin(8, 0, 1, night, 4001 + race_index)
            a.go()
            b.go()
            tick(500)
            native.check(a.snapshot().raceTicks > 0 and b.snapshot().raceTicks > 0, "released source race")
            state("night initial" if night else "day initial / new-race state reset", night, night)
            tick(host_key=72, lights=not args.expect_baseline_bug)
            tick(65, lights=not args.expect_baseline_bug)
            if args.expect_baseline_bug:
                native.check(not (a.snapshot().flags & 16), "baseline local H switched headlights off")
                native.check(bool(b.remote().flags & 16), "baseline opponent remains incorrectly on")
                report["cases"].append({"name": "reproduced: local off but opponent on",
                                        "local": describe(a.snapshot()), "opponent": describe(b.remote())})
                a.leave()
                b.leave()
                break
            state("host toggled independently", not night, night)
            tick(join_key=72)
            tick(65)
            state("guest toggled independently", not night, not night)
            tick(host_key=72)
            tick(65)
            state("host restored independently", night, not night)
            tick(join_key=72)
            tick(65)
            state("both restored", night, night)

            # Input packet loss must not permanently lose a cosmetic toggle.
            tick(host_key=72, lights=False)
            tick(4, lights=False)
            native.check(bool(b.remote().flags & 16) == night, "lost state retains last known remote lamps")
            tick(65)
            state("next absolute state recovers lost toggle", not night, night)
            tick(host_key=72)
            tick(65)
            state("restored after loss", night, night)

            # State API must not mutate either physics pose, result, or clock.
            before = b.authority()
            before_physics = bytes(b.remote())[40:116]
            sequence = 10_000_000
            native.check(b.set_lights(sequence, not night) == 1, "accept newer absolute state")
            native.check(b.set_lights(sequence, night) == 1, "ignore duplicate conflicting state")
            native.check(b.set_lights(sequence - 1, night) == 1, "ignore stale conflicting state")
            tick(65, lights=False)
            native.check(bool(b.remote().flags & 16) == (not night), "stale/duplicate cannot revert state")
            report["cases"].append({"name": "stale and duplicate ignored", "opponent": describe(b.remote())})
            native.check(b.set_lights(sequence + 1, night) == 1, "newer state restores lamps")
            # Use no scene step here, isolating the presentation API itself.
            before = b.authority()
            before_physics = bytes(b.remote())[40:116]
            native.check(b.set_lights(sequence + 2, not night) == 1, "presentation-only change accepted")
            after = b.authority()
            native.check(bytes(before) == bytes(after), "lamp state does not change authority, results or clocks")
            native.check(before_physics == bytes(b.remote())[40:116], "lamp state does not change remote physics pose")
            native.check(b.set_lights(sequence + 3, night) == 1, "restore presentation state")
            native.check(b.set_lights(0, 1) == 0 and b.set_lights(sequence + 4, 2) == 0,
                         "zero sequence and non-boolean state rejected")
            native.check(a.authority().verified > 500 and b.authority().verified > 500,
                         "both numerical simulations remain mutually verified")
            for client in clients:
                native.check(native.hashes(client.saves) == client.before, "headlights/racing did not write saves or records")
                client.leave()
                native.check(native.hashes(client.saves) == client.before, "leaving preserved saves and records")
            report["cases"].append({"name": "physics and private saves unchanged"})
        report["passed"] = True
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        for client in clients:
            client.stop()
        report["checks"] = native.checks
        report["seconds"] = round(time.time() - started, 3)
        (output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps({k: v for k, v in report.items() if k != "cases"}, indent=2))


if __name__ == "__main__":
    main()
