"""Exercise the shipped setup entry points under Windows PowerShell 5.1.

Nonempty destination sentinels stop setup before network access. Unlike
dot-sourcing the extraction helpers, -File exercises parameter binding.
"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import uuid


repo = Path(__file__).resolve().parents[2]
proof = repo / 'Verification' / ('setup-launcher-' + uuid.uuid4().hex)
proof.mkdir(parents=True)
shell = Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
cmd = Path(os.environ['SystemRoot']) / 'System32/cmd.exe'
env = {k: v for k, v in os.environ.items() if k.lower() != 'psmodulepath'}
results = []

for name in ('cmd-default', 'powershell-default', 'powershell-explicit'):
    case = proof / name
    setup = case / "Setup Japanese \u65e5\u672c\u8a9e & spaces !"
    setup.mkdir(parents=True)
    for filename in ('Install Initial D.cmd', 'Install-InitialD.ps1'):
        shutil.copyfile(repo / 'Tools' / filename, setup / filename)
    caller = case / 'unrelated working directory'
    caller.mkdir()
    target = (case / 'Custom destination \u65e5\u672c\u8a9e & spaces !'
              if name == 'powershell-explicit' else setup / 'Initial D Arcade Stage 3')
    target.mkdir()
    sentinel = target / 'existing-save.txt'
    sentinel.write_bytes(b'Existing player data must survive.')
    if name == 'cmd-default':
        # Match double-clicking the shipped batch file; feed its final pause.
        command = f'"{cmd}" /d /s /c ""{setup / "Install Initial D.cmd"}""'
    else:
        command = [str(shell), '-NoProfile', '-ExecutionPolicy', 'Bypass',
                   '-File', str(setup / 'Install-InitialD.ps1')]
        if name == 'powershell-explicit':
            command += ['-Destination', str(target)]
    run = subprocess.run(command, cwd=caller, env=env, input='\n',
                         capture_output=True, text=True, errors='replace',
                         timeout=45, creationflags=subprocess.CREATE_NO_WINDOW)
    output = run.stdout + run.stderr
    (case / 'output.txt').write_text(output, encoding='utf-8')
    assert run.returncode == 1, (name, run.returncode, output)
    assert 'The installation folder must be empty.' in output, (name, output)
    assert 'Checking the official release' not in output, (name, output)
    assert 'Cannot bind argument' not in output, (name, output)
    assert sentinel.read_bytes() == b'Existing player data must survive.', name
    assert list(target.iterdir()) == [sentinel], name
    assert not list(caller.iterdir()), name
    assert not list(setup.glob('.initial-d-download-*')), name
    if name == 'powershell-explicit':
        assert not (setup / 'Initial D Arcade Stage 3').exists(), name
    results.append({'test': name, 'passed': True, 'exitCode': run.returncode})
    print(name + ': PASS', flush=True)

(proof / 'report.json').write_text(json.dumps({'passed': True, 'tests': results}, indent=2))
print('Evidence: ' + str(proof))
