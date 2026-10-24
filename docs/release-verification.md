# Release verification

Use one entry point for the maintained offline regression suite and candidate identification:

```powershell
python -X utf8 Tools/verify_release.py --suite source
python -X utf8 Tools/verify_release.py --suite candidate --player D:/releases/candidate
```

Windows, Python 3.11+, CMake, Visual Studio C++ and Roslyn/.NET Framework, and the patched Node 24 range in `Leaderboard/package.json` are required. Candidate localization also needs the Unity Mono runtime matching `ProjectSettings/ProjectVersion.txt`. Override discovery with `--node`, `--compiler`, or `--mono` as needed. The candidate path is mandatory; the command never assumes which folder named Current is intended.

Each invocation creates a new directory under `Verification/`, or the new directory supplied with `--output`. Existing evidence is never overwritten. `summary.json` records the source commit and code/config fingerprint, compiler/runtime identity, each command, log, exit status and result-report hash. A failed command, missing/negative result report, timeout, or changed source/candidate makes the command fail. Fix the cause and run into a new evidence directory.

The source suite runs the verifier's own failure/privacy regressions, the backend tests, thirteen managed multiplayer suites, three package privacy/migration suites, and the setup-launcher scenarios. Tests use disposable files and simulated service boundaries. They do not require production credentials, submit times, or ban users. `.github/workflows/source-verification.yml` runs the same command for main-branch pushes and pull requests, retaining evidence even after failure.

The workflow pins the v7 revisions verified on October 23, 2026 from the official [checkout](https://github.com/actions/checkout), [setup-python](https://github.com/actions/setup-python), [setup-node](https://github.com/actions/setup-node) and [upload-artifact](https://github.com/actions/upload-artifact) repositories. It uses read-only repository permissions and does not persist checkout credentials. Its [first clean Windows run](https://github.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined/actions/runs/40038959060) passed for commit `e80d013`. Every subsequent change still needs its own run.

The candidate suite runs those source checks plus production localization and installed-build compatibility checks. It hashes every distributable file before and after verification using the existing release packaging allowlist. `candidate.json` records individual hashes and an aggregate inventory hash. Use a clean candidate without original ROMs or player saves: ROM payloads and saves are rejected before hashing, personal music is pruned, and generated ROM instructions are excluded. No game executable is launched by this initial candidate suite.

**Passing the source/candidate suite does not certify a release.** `releaseReady` remains false and `pendingGates` lists missing evidence. An existing assembly's hash identifies it; it does not prove it was built from the current source. The fresh-build command below records that additional provenance and selected native tests. Packaged gameplay diagnostics, actual two-PC online cycles, install/update journeys, and the hardware/presentation matrix are still required. Partial evidence stays explicit. The packager and publisher have not yet been connected to a complete release gate.

Keep the source/config fingerprint and candidate inventory with the eventual release evidence. A successful source run against one checkout must not be reused to approve a different player. See the [stabilization sequence](project-stabilization-audit-2026-10-23.md) for the remaining work and acceptance criteria.

## Fresh native and Unity candidate

```powershell
python -X utf8 Tools/build_release_candidate.py --original-image D:/reference/idas3_main_0C020000.bin --output Verification/fresh-candidate
```

This creates a new native build, compiles both native plugins and the asset packer, runs eleven selected native tests, and builds a full Windows player into `Verification/fresh-candidate/player`. It uses the exact Unity version recorded in the project and installed Visual Studio C++ tools. The local original image is used only by native reference tests; it is never copied into the player. Existing evidence and player folders are refused. Neither the desktop game nor the checked-in native DLLs is replaced.

The native DLLs in the player must match the newly compiled outputs. Evidence includes compiler/editor identities, source and content/config input hashes before and after the build, native test XML, the Unity build result, staging manifest, and the distributable inventory. Bundled SDKs and the embedded updater remain recorded binary inputs; this is not a claim that every dependency was rebuilt. Local EOS client configuration is hashed without recording its contents. Keep this local build evidence private; it contains machine paths and local configuration fingerprints.

The release entry point uses the project's saved configuration without rewriting it. If the configuration is unsupported, configure/review the project first. If source or input bytes change during the build, the run fails and the evidence is retained. A successful fresh build is an intermediate gate: run candidate verification and packaged integration next, followed by the remaining acceptance checks above.

## Packaged local gameplay

```powershell
python -X utf8 Tools/verify_packaged_player.py --build-evidence Verification/fresh-candidate --rom D:/reference/gds-0033.chd --output Verification/player-check
```

This verifies the candidate against its successful fresh-build evidence, copies only distributable files into a **private test player**, and runs the actual executable with separate test profiles. The original candidate and desktop installation are never launched or changed. The explicit local CHD is copied only into the private player; the game still validates it. Do not upload the evidence directory or private player: it contains original game data. Both the ROM and the `PRIVATE_TEST_ONLY.txt` marker cause release packaging to reject that player.

The default `all` suite checks blocked startup and its real GUI capture, mode flow, two local TCP rematch scenarios, and abrupt socket loss. The paired tests use shared native authority, natural timeouts and normal Continue inputs. They cover Yes/Yes followed by a second race, a peer choosing No while the other remains at points, and No after the other has chosen Yes. Every process must return a positive report and shut down cleanly. Failed cases remain failed while independent cases continue; timeouts terminate only owned processes. The distributable bytes in both the private copy and clean candidate are checked again afterward.

Use `--suite startup` to run only the missing-ROM check without supplying a ROM, or `--suite flows` for mode flow and the three local pairs. Unselected checks remain listed as pending. These are hidden Windows processes: native camera captures are available, but the paired tests explicitly omit actual window/IMGUI pixels. The missing-ROM GUI capture is still required by the startup suite; a hidden-window capture timeout fails that suite even if its startup-state assertions pass.

These checks do **not** establish Internet/EOS matchmaking, nonzero win/loss reward persistence, every finish order, physical controller recovery, or all-language/resolution presentation. Those acceptance gates remain separate. The existing mode-flow diagnostic also includes controlled visual fixtures; it is not a human-driven complete race or original-game parity measurement.

## Fresh-install extraction and failure recovery

The source suite runs the real PowerShell setup extraction functions against disposable two-archive fixtures. It checks damaged compressed data, late extraction errors, a locked destination, retries, paths with trailing separators, existing player files, archive validation, and cleanup containment. The installer extracts into a unique sibling directory and publishes it only after both archives finish. A caught extraction or publication failure cleans only that invocation's staging folder and leaves the destination absent or empty. Existing player files are refused before extraction.

For a full-sized install check against the exact fresh candidate:

```powershell
python -X utf8 Tests/Updates/test_setup_extraction.py --build-evidence Verification/fresh-candidate --report-dir Verification/setup-candidate
```

This also creates core/course ZIPs from the shipping allowlist, checks both CRCs and download-size limits, runs the actual setup extraction, and hashes every installed file against the candidate before and after the operation. It snapshots the tested installer in the evidence directory and records its hash and extraction duration. To reuse archives from an earlier test, add `--archives <core.zip> <courses.zip>`; their inventory and installed bytes must still match the fresh-build evidence.

It does not download from GitHub, launch the game, or exercise an update of an existing installation. Abrupt process termination/power loss and the native update helper's recovery still require separate acceptance; caught-error tests are not substitutes for those cases.

## Native update recovery and setup packaging

The source suite now builds the native updater with the selected Visual Studio C++ toolchain. It separately runs normal installation/rollback/handoff and interruption/recovery checks against that build and the exact binary embedded in `Assets/Resources/UpdateInstaller.exe.bytes`. Both must pass. Recovery checks use disposable files, verify all original hashes and private-file preservation, reject invalid plans/status/backups, and forcibly terminate only test-owned processes during installation and during recovery. A repeated recovery must complete correctly. These process-termination checks do not simulate storage failure or power loss.

The setup recovery launcher is exercised through actual CMD and Windows PowerShell entry points with isolated `TEMP` caches, including a missing game executable, Japanese/spaced paths, multiple installations, cancellation and a damaged backup. Managed cache checks establish that failed recoveries retain backups and verified recoveries permit cleanup of both downloads. The desktop restart-failure message box is explicitly outside the automated native installer suite.

`Tools/Build-SetupPackage.py` requires the native installation report, full recovery report and launcher report before creating a new setup ZIP. It rejects mismatched helper/launcher hashes, missing or failed cases and omitted process-interruption tests. Example:

```powershell
python Tools/Build-SetupPackage.py --helper <tested-helper.exe> --archive <new-setup.zip> --installation-report <native-windows-report.json> --recovery-report <recovery-report.json> --launcher-report <launcher-report.json>
```

The ZIP contains six explicit files: installation and recovery CMD/PowerShell pairs, the verified native helper and a README. Creation does not publish an update. Players run `Recover Initial D.cmd` from the extracted setup ZIP; the old game need not launch. Normal game startup does not automatically run recovery. A new full candidate is still required after updating the embedded helper or managed cache code.
