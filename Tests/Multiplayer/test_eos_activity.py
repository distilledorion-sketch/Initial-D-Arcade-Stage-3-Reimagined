"""Run the actual EOS census against a fake SDK boundary; no live service or player data."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from test_build_compatibility import find_compiler

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler')
    parser.add_argument('--report-dir', type=Path, default=ROOT / 'Verification/eos-online-20261011/activity-unit')
    args = parser.parse_args()
    report = args.report_dir.resolve()
    report.mkdir(parents=True, exist_ok=True)
    production = [ROOT / 'Assets/Scripts/Multiplayer' / name for name in ('Idas3EosActivity.cs', 'Idas3OnlineActivity.cs')]
    sources = production + [Path(__file__).with_name(name) for name in ('EosActivityFakes.cs', 'EosActivityDriver.cs')]
    exe = report / 'EosActivityDriver.exe'
    compiled = subprocess.run([str(find_compiler(args.compiler)), '/nologo', '/warnaserror+', '/target:exe', '/out:' + str(exe), *map(str, sources)], capture_output=True, text=True, encoding='utf-8')
    (report / 'compile.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8')
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    outputs = []
    checks = []
    for name in ('summary', 'publish', 'survey', 'late-create', 'late-query', 'late-update', 'pacing'):
        ran = subprocess.run([str(exe), name], capture_output=True, text=True, encoding='utf-8', timeout=15)
        outputs.append(name + '\n' + ran.stdout + ran.stderr)
        checks.extend(line.split('\t', 1)[1] for line in ran.stdout.splitlines() if line.startswith('PASS\t'))
        if ran.returncode:
            (report / 'checks.log').write_text('\n'.join(outputs), encoding='utf-8')
            raise RuntimeError(outputs[-1])
    (report / 'checks.log').write_text('\n'.join(outputs), encoding='utf-8')
    (report / 'report.json').write_text(json.dumps({'passed': True, 'count': len(checks), 'checks': checks, 'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in production}}, indent=2) + '\n', encoding='utf-8')
    print(f'PASS {len(checks)} actual EOS activity checks; report: {report / "report.json"}')


if __name__ == '__main__':
    main()
