"""Run production EOS packet framing against boundary, loss and malformed input cases."""
import hashlib
import json
from pathlib import Path
import subprocess
from test_build_compatibility import find_compiler

ROOT = Path(__file__).resolve().parents[2]
proof = ROOT / "Verification/eos-online-20261011/packets"
proof.mkdir(parents=True, exist_ok=True)
source = ROOT / "Assets/Scripts/Multiplayer/Idas3EosPackets.cs"
driver = Path(__file__).with_name("EosPacketsDriver.cs")
exe = proof / "EosPacketsDriver.exe"
compiled = subprocess.run([str(find_compiler(None)), "/nologo", "/warnaserror+", "/target:exe",
    "/out:" + str(exe), str(source), str(driver)], capture_output=True, text=True)
(proof / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
if compiled.returncode:
    raise RuntimeError(compiled.stdout + compiled.stderr)
ran = subprocess.run([str(exe)], capture_output=True, text=True)
(proof / "checks.log").write_text(ran.stdout + ran.stderr, encoding="utf-8")
checks = [s.split("\t", 1)[1] for s in ran.stdout.splitlines() if s.startswith("PASS\t")]
(proof / "report.json").write_text(json.dumps({"passed": ran.returncode == 0,
    "checks": checks, "count": len(checks), "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    "scope": "Actual managed production framing and reassembly; no live EOS service or network involved."}, indent=2) + "\n", encoding="utf-8")
if ran.returncode:
    raise RuntimeError(ran.stdout + ran.stderr)
print(f"PASS {len(checks)} EOS packet checks; report: {proof / 'report.json'}")
