"""Drive a fresh card through the installed regional menus with isolated saves and no audio device."""
import argparse
import json
from pathlib import Path
import sys

from check_native_multiplayer import Client
sys.path.insert(0, str(Path(__file__).resolve().parent / "GameSetup"))
from disc_setup import receipt_region


def first_race(installation, evidence):
    data = installation / "InitialDUnity_Data"
    cache = data / "StreamingAssets/IDAS3"
    receipt = json.loads((cache / "disc-receipt.json").read_text(encoding="utf-8"))
    region = receipt_region(receipt)
    if region is None:
        raise ValueError("The installation has no recognized regional disc receipt.")
    evidence.mkdir(parents=True, exist_ok=False)
    client = Client(evidence, "driver", data / "Plugins/x86_64/Idas3Unity.dll",
                    assets=cache, language=int(region == "gds-0032c"))
    stages = []
    try:
        for _ in range(180):
            client.step(throttle=False, key=13)
            for _ in range(75):
                client.step(dt=1/30, throttle=False)
                status = client.status()
                if not stages or status.stage != stages[-1]:
                    stages.append(status.stage)
            if not status.flags & 1:
                break
        # Save, make, car, transmission, tuning, name, mode, course and rival.
        expected = [1, 2, 3, 4, 12, 11, 5, 6, 10]
        if stages not in (expected, [0, *expected]):
            raise AssertionError(f"Unexpected first-card menu order: {stages}")
        for _ in range(2400):
            client.step(dt=1/30)
            if client.status().racePhase == 2:
                break
        for _ in range(180):
            client.step(dt=1/30)
        status = client.status()
        card = client.saves / "saves/slot_1/save.txt"
        if status.flags & 1 or status.racePhase != 2 or status.speed <= 1 or not card.is_file():
            raise AssertionError("The new card did not save and reach a drivable race.")
        result = {"region": region, "stages": stages, "speedMetresPerSecond": status.speed,
                  "ticks": status.ticks, "save": card.relative_to(evidence).as_posix()}
        (evidence / "first-race.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(f"{region}: new card saved, race started, speed {status.speed:.1f} m/s")
    finally:
        client.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("installation", type=Path)
    parser.add_argument("evidence", type=Path, help="New directory for test saves and results")
    args = parser.parse_args()
    first_race(args.installation.resolve(), args.evidence.resolve())
