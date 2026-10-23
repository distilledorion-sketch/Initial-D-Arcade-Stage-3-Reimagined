# Release verification

Use one entry point for the maintained offline regression suite and candidate identification:

```powershell
python -X utf8 Tools/verify_release.py --suite source
python -X utf8 Tools/verify_release.py --suite candidate --player D:/releases/candidate
```

Windows, Python 3.11+, Visual Studio Roslyn/.NET Framework, and the patched Node 24 range in `Leaderboard/package.json` are required. Candidate localization also needs the Unity Mono runtime matching `ProjectSettings/ProjectVersion.txt`. Override discovery with `--node`, `--compiler`, or `--mono` as needed. The candidate path is mandatory; the command never assumes which folder named Current is intended.

Each invocation creates a new directory under `Verification/`, or the new directory supplied with `--output`. Existing evidence is never overwritten. `summary.json` records the source commit and code/config fingerprint, compiler/runtime identity, each command, log, exit status and result-report hash. A failed command, missing/negative result report, timeout, or changed source/candidate makes the command fail. Fix the cause and run into a new evidence directory.

The source suite runs the verifier's own failure/privacy regressions, the backend tests, thirteen managed multiplayer suites, three package privacy/migration suites, and the setup-launcher scenarios. Tests use disposable files and simulated service boundaries. They do not require production credentials, submit times, or ban users. `.github/workflows/source-verification.yml` runs the same command for main-branch pushes and pull requests, retaining evidence even after failure.

The workflow pins the v7 revisions verified on October 23, 2026 from the official [checkout](https://github.com/actions/checkout), [setup-python](https://github.com/actions/setup-python), [setup-node](https://github.com/actions/setup-node) and [upload-artifact](https://github.com/actions/upload-artifact) repositories. It uses read-only repository permissions and does not persist checkout credentials. A local run verifies the test command; the hosted workflow still needs its first GitHub run after publication.

The candidate suite runs those source checks plus production localization and installed-build compatibility checks. It hashes every distributable file before and after verification using the existing release packaging allowlist. `candidate.json` records individual hashes and an aggregate inventory hash. Use a clean candidate without original ROMs or player saves: ROM payloads and saves are rejected before hashing, personal music is pruned, and generated ROM instructions are excluded. No game executable is launched by this initial candidate suite.

**Passing these checks does not certify a release.** `releaseReady` remains false and `pendingGates` lists missing evidence. An existing assembly's hash identifies it; it does not prove it was built from the current source. Fresh build provenance, native reference tests, packaged gameplay diagnostics, actual two-PC online cycles, install/update journeys, and the hardware/presentation matrix must be added before the full release gate can be satisfied. This first implementation makes partial evidence explicit instead of silently treating it as complete. The packager and publisher have not yet been connected to this gate.

Keep the source/config fingerprint and candidate inventory with the eventual release evidence. A successful source run against one checkout must not be reused to approve a different player. See the [stabilization sequence](project-stabilization-audit-2026-10-23.md) for the remaining work and acceptance criteria.
