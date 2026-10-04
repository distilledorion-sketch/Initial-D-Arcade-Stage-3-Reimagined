"""Exercise packet-outage recovery through two actual native game DLL instances.

Private save folders; no Steam or live player traffic. The C++ timeline test
checks exact state/effect hashes; this test checks the scene bridge can stall,
keep exchanging packets, resume driving and leave without changing saves.
"""
import argparse
import hashlib
import json
from pathlib import Path
from test_opponent_headlights import Client, native, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    dll = ROOT / 'Assets/Plugins/x86_64/Idas3Unity.dll'
    clients = []
    report = {'passed': False, 'dll_sha256': hashlib.sha256(dll.read_bytes()).hexdigest(), 'cases': []}
    try:
        clients.append(Client(output, 'host', dll, False))
        clients.append(Client(output, 'join', dll, False))
        for outage in range(3):
            a, b = clients
            a.begin(0, 8, 0, False, 9300 + outage)
            b.begin(8, 0, 1, False, 9300 + outage)
            a.go()
            b.go()
            for _ in range(1000):
                a.step()
                b.step()
                a.send_to(b)
                b.send_to(a)
                if all(c.snapshot().raceTicks > 0 for c in clients):
                    break
            native.check(all(c.snapshot().raceTicks > 0 for c in clients), 'Both scenes passed the showcase/countdown into a live race')
            stalls = [0, 0]
            for wall in range(780):
                for slot, client in enumerate(clients):
                    client.step()
                    stalls[slot] += bool(client.authority().stalled)
                for receiver in range(2):
                    if 350 <= wall < 530 and outage in (receiver, 2):
                        continue
                    clients[1 - receiver].send_to(clients[receiver])
            # A stalled endpoint can be a few frames behind. Bring both to the
            # same target using normal scene steps before comparing their ACKs.
            target = max(c.authority().frame for c in clients) + 30
            for _ in range(180):
                for client in clients:
                    if client.authority().frame < target:
                        client.step()
                a.send_to(b)
                b.send_to(a)
                if all(c.authority().frame == target for c in clients):
                    break
            for _ in range(6):
                a.send_to(b)
                b.send_to(a)
            status = [client.authority() for client in clients]
            native.check(any(stalls), 'Outage must stall at least one actual scene')
            native.check(status[0].frame == status[1].frame, 'Both scenes catch up to the same frame')
            native.check(all(s.frame == s.confirmed == s.verified for s in status), 'Both scenes verify all recovered input')
            native.check(all(s.winner == -3 for s in status), 'Temporary outage must not award a race result')
            before = status[0].frame
            for _ in range(30):
                a.step()
                b.step()
                a.send_to(b)
                b.send_to(a)
            native.check(all(c.authority().frame > before for c in clients), 'Both scenes resume simulation after recovery')
            for client in clients:
                native.check(client.status().flags & 128 and not client.status().flags & 2048, 'Race remains online, without disconnect flag')
                client.leave()
                native.check(native.hashes(client.saves) == client.before, 'Recovered unfinished race preserves private saves')
            report['cases'].append({'outage': outage, 'stall_frames': stalls, 'verified_frame': before, 'resumed': True})
            print('PASS native scene outage', outage, 'verified frame', before, 'stalls', stalls)
        report['passed'] = True
    finally:
        for client in clients:
            client.stop()
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    assert report['passed']


if __name__ == '__main__':
    main()
