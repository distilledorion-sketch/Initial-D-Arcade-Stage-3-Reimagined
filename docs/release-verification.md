# Release verification

Use one entry point for the maintained offline regression suite and candidate identification:

```powershell
python -X utf8 Tools/verify_release.py --suite source
python -X utf8 Tools/verify_release.py --suite candidate --player D:/releases/candidate
```

Windows, Python 3.11+, CMake, Visual Studio C++ and Roslyn/.NET Framework, and the patched Node 24 range in `Leaderboard/package.json` are required. Candidate localization also needs the Unity Mono runtime matching `ProjectSettings/ProjectVersion.txt`. Override discovery with `--node`, `--compiler`, or `--mono` as needed. The candidate path is mandatory; the command never assumes which folder named Current is intended.

Each invocation creates a new directory under `Verification/`, or the new directory supplied with `--output`. Existing evidence is never overwritten. `summary.json` records the source commit and code/config fingerprint, compiler/runtime identity, each command, log, exit status and result-report hash. A failed command, missing/negative result report, timeout, or changed source/candidate makes the command fail. Fix the cause and run into a new evidence directory.

The source suite runs the verifier's own failure/privacy regressions, the backend tests, fifteen managed multiplayer suites, three package privacy/migration suites, and the setup-launcher scenarios. Tests use disposable files and simulated service boundaries. They do not require production credentials, submit times, or ban users. `.github/workflows/source-verification.yml` runs the same command for main-branch pushes and pull requests, retaining evidence even after failure.

The workflow pins the v7 revisions verified on October 23, 2026 from the official [checkout](https://github.com/actions/checkout), [setup-python](https://github.com/actions/setup-python), [setup-node](https://github.com/actions/setup-node) and [upload-artifact](https://github.com/actions/upload-artifact) repositories. It uses read-only repository permissions and does not persist checkout credentials. Its [first clean Windows run](https://github.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined/actions/runs/40038959060) passed for commit `e80d013`. Every subsequent change still needs its own run.

The candidate suite runs those source checks plus production localization and installed-build compatibility checks. It hashes every distributable file before and after verification using the existing release packaging allowlist. `candidate.json` records individual hashes and an aggregate inventory hash. Use a clean candidate without original ROMs or player saves: ROM payloads and saves are rejected before hashing, personal music is pruned, and generated ROM instructions are excluded. No game executable is launched by this initial candidate suite.

**Passing the source/candidate suite does not certify a release.** `releaseReady` remains false and `pendingGates` lists missing evidence. An existing assembly's hash identifies it; it does not prove it was built from the current source. The fresh-build command below records that additional provenance and selected native tests. Packaged gameplay diagnostics, actual two-PC online cycles, install/update journeys, and the hardware/presentation matrix are still required. Partial evidence stays explicit. Public packaging and the maintained draft-publication command require the acceptance record below.

Keep the source/config fingerprint and candidate inventory with the eventual release evidence. A successful source run against one checkout must not be reused to approve a different player. See the [stabilization sequence](project-stabilization-audit-2026-10-23.md) for the remaining work and acceptance criteria.

## Acceptance record and packaging

`Tools/release_acceptance.py` joins the existing evidence without running the game or publishing anything. Its `check()` function is also required by public `Build-ReleasePackage.py` packaging. It independently hashes the candidate, requires committed matching source/build inputs, revalidates the complete candidate suite and underlying report hashes, reuses the packaged/scored runtime validators, checks failed-save recovery captures and startup boundaries, and checks full install/upgrade evidence and the exact two download inventories. Missing, stale, failed, contradictory or incomplete evidence rejects the operation. The existing native tests and their XML are checked; full native and real-world acceptance additionally require the review below.

```powershell
python -X utf8 Tools/release_acceptance.py --manifest Verification/release/acceptance.json --player Verification/fresh-candidate/player --output Verification/release/acceptance-result.json
python -X utf8 Tools/Build-ReleasePackage.py --player Verification/fresh-candidate/player --archive <new-release.zip> --acceptance Verification/release/acceptance.json
```

The manifest uses schema `idas3-release-acceptance-v1`, the exact `sourceCommit`, `applicationVersion` and `candidateInventorySha256`, and `reports` with all seven keys: `build`, `candidate`, `packaged`, `scored`, `startupRecovery`, `freshInstall`, `upgrade`. Each value is `{ "path": "path/to/summary-or-report.json", "sha256": "the-file-sha256" }`. Paths resolve relative to the manifest; explicit absolute local paths are also supported. `build` must reference the full builder's `summary.json`; `packaged` must be the complete `all` suite; `scored` must include `--result-recovery`. The startup and install entries must reference the maintained runners' actual results. A focused updater run or a private scripts-only diagnostic cannot replace these requirements.

`review` references a separate hashed JSON record with schema `idas3-release-review-v1`, the same commit/version/inventory, `reviewer`, a timezone-qualified `reviewedAt`, `passed`, and a `checks` array. The required IDs and their detailed criteria are the `REVIEW_CHECKS` table in `Tools/release_acceptance.py`: native-reference, quick-match, room-code, online-recovery, moving-finishers, input-ownership, save-interruption, published-updater, public-install, presentation, performance, arcade-behavior and identity-moderation. Every row requires `id`, `passed`, written `observations`, and a nonempty `evidence` list of path/SHA-256 references to actual supporting logs, measurements or captures. Keep unperformed rows false. A local TCP pass cannot approve a different-network EOS row. Do not manufacture observations to satisfy the schema.

The validator checks the completeness, attribution and unchanged bytes of this review; it cannot establish the truth of arbitrary handwritten claims. The reviewer remains responsible for inspecting the supporting evidence and meeting the named criteria. This local record is not a cryptographic approval service. Keep paths, logs, private players, ROMs and all acceptance evidence out of release assets.

For isolated archive tests before acceptance, use `--candidate-only` and a filename beginning with `Candidate-`. Those packages explicitly report `releaseReady: false`. No package or report is overwritten. Public packaging rechecks acceptance rather than trusting a previous `releaseReady` result, then compares the ZIP payload with the accepted inventory; a failure produces no approved package report. A single public ZIP above the asset size limit is refused; the verified split downloads are still needed for the complete game. These tools enforce the maintained process; repository administrators can still bypass them manually.

### Publishing an accepted draft

Prepare and upload assets to a draft before this final step. `Tools/publish_verified_release.py` does not upload files, create releases, change tags or edit release notes. Its default mode is read-only:

```powershell
python -X utf8 Tools/publish_verified_release.py --plan Verification/release/publication.json --player Verification/fresh-candidate/player --output Verification/release/draft-check.json
```

The private plan uses schema `idas3-publication-plan-v1`. Required fields are `repository` (the exact game repository), numeric `releaseId` (the existing draft), numeric `baselineReleaseId` (the reviewed current stable release), `tag`, `sourceCommit`, `title`, and hashed `notes` and `acceptance` references. The acceptance reference is the complete manifest, not a cached successful result. Each file reference uses `{ "path": "local/path", "sha256": "lowercase-sha256" }`, relative to the plan or explicitly absolute.

`assets` has exactly `core`, `courses`, `setup`, `checksums` and `patches`. The first four are file references with the public filenames generated by `version_names()` (checksums is `SHA256SUMS.txt`). `patches` is an array of file references, each also containing `baseVersion` and a `baseDownloads` array of references to that version's original full downloads. Include every patch base supported by the previous stable release, plus the previous stable version itself. Do not silently drop older supported installations.

The command reruns full acceptance, checks both full downloads against the tested archive hashes and exact candidate inventory, checks all six setup entries against the tested package and current source, and verifies the setup script is pinned to the target version. Every patch must reconstruct the exact candidate from its matching published baseline; allowed legacy-file retirement rules still apply. Every public asset must appear in the checksum table. GitHub names, sizes, uploaded state and SHA-256 digests must match, with no extra transfer ZIPs. The draft's title, notes, tag, full commit SHA and remote commit tree must match the reviewed plan. An existing tag must resolve to that commit.

After reviewing the read-only result, use the same command with `--publish` and a **new** output path. It repeats acceptance and remote checks immediately before its sole write: publishing the specified draft as the latest stable release. The report records that a write is about to be attempted. If the response is lost or verification fails afterward, use `--verify-published` with a new report to inspect the existing release; do not blindly repeat the write. That recovery mode does not publish anything.

Keep other uploaders stopped during the final step: GitHub provides no transaction spanning release metadata and asset changes. The tool's simulated API/archive tests verify decisions and failure handling; they are not evidence of an actual publication or network reliability. Keep plans and reports private. No current candidate is approved merely because these tool tests pass.

## Fresh native and Unity candidate

```powershell
python -X utf8 Tools/build_release_candidate.py --original-image D:/reference/idas3_main_0C020000.bin --output Verification/fresh-candidate
```

This creates a new native build, compiles both native plugins and the asset packer, runs fifteen selected native tests, and builds a full Windows player into `Verification/fresh-candidate/player`. It uses the exact Unity version recorded in the project and installed Visual Studio C++ tools. The local original image is used only by native reference tests; it is never copied into the player. Existing evidence and player folders are refused. Neither the desktop game nor the checked-in native DLLs is replaced.

The native DLLs in the player must match the newly compiled outputs. Evidence includes compiler/editor identities, source and content/config input hashes before and after the build, native test XML, the Unity build result, staging manifest, and the distributable inventory. Bundled SDKs and the embedded updater remain recorded binary inputs; this is not a claim that every dependency was rebuilt. Local EOS client configuration is hashed without recording its contents. Keep this local build evidence private; it contains machine paths and local configuration fingerprints.

The release entry point uses the project's saved configuration without rewriting it. If the configuration is unsupported, configure/review the project first. If source or input bytes change during the build, the run fails and the evidence is retained. A successful fresh build is an intermediate gate: run candidate verification and packaged integration next, followed by the remaining acceptance checks above.

## Packaged local gameplay

```powershell
python -X utf8 Tools/verify_packaged_player.py --build-evidence Verification/fresh-candidate --rom D:/reference/gds-0033.chd --output Verification/player-check
```

This verifies the candidate against its successful fresh-build evidence, copies only distributable files into a **private test player**, and runs the actual executable with separate test profiles. The original candidate and desktop installation are never launched or changed. The explicit local CHD is copied only into the private player; the game still validates it. Do not upload the evidence directory or private player: it contains original game data. Both the ROM and the `PRIVATE_TEST_ONLY.txt` marker cause release packaging to reject that player.

The default `all` suite checks blocked startup and its real GUI capture, mode flow, two local TCP rematch scenarios, abrupt socket loss, and updater/session coexistence. The paired tests use shared native authority, natural timeouts and normal Continue inputs. They cover Yes/Yes followed by a second race, a peer choosing No while the other remains at points, and No after the other has chosen Yes. Every process must return a positive report and shut down cleanly. Failed cases remain failed while independent cases continue; timeouts terminate only owned processes. The distributable bytes in both the private copy and clean candidate are checked again afterward.

Use `--suite startup` to run only the missing-ROM check without supplying a ROM, `--suite flows` for mode flow, the three local pairs and the updater check, or `--suite updater` for just the updater check. Unselected checks remain listed as pending. These are hidden Windows processes: native camera captures are available, but the paired tests explicitly omit actual window/IMGUI pixels. The missing-ROM GUI capture is still required by the startup suite; a hidden-window capture timeout fails that suite even if its startup-state assertions pass.

The updater case opens the real dialog through attract Options while hosting a private loopback room. A TCP peer must be accepted and receive production session traffic while the dialog remains open. Update/Repair attempts are intercepted and must be blocked until leaving the room; keyboard Update and same-version Full Repair must then work. The validator requires these specific observations, not a generic attract-menu pass. This does not install an update, contact EOS, or verify the warning's pixels.

Add `--updater-captures` to require actual updater `OnGUI` Repaints in English, Japanese and Chinese at 640x480, 1024x768, 1280x720 and 1920x800. A hidden or minimized window may need to be brought forward so Unity produces Repaints. A missing capture, blank runtime image, or unavailable requested size fails the check. The dialog is captured against black; native-background composition is outside this check. The runner records image hashes and verifies dimensions; inspect the images separately for text clipping and readability.

These checks do **not** establish Internet/EOS matchmaking, nonzero win/loss reward persistence, every finish order, physical controller recovery, or all-language/resolution presentation. Those acceptance gates remain separate. The existing mode-flow diagnostic also includes controlled visual fixtures; it is not a human-driven complete race or original-game parity measurement.

## Scored races and connected rematch

Run the separate earned-reward acceptance against a fresh candidate containing the scored diagnostic:

```powershell
python -X utf8 Tools/verify_scored_player.py --build-evidence Verification/fresh-candidate --rom <local-gds-0033.chd> --output Verification/scored-candidate
```

The runner copies and verifies the complete candidate in a marked private directory. Two local TCP players use newly generated tuned saves, normal wheel input, and unchanged wall-clock timing. A read-only path follower drives the host to Myogi's normal finish, then the guest wins the connected rematch. The other driver stays stationary and times out. No finish flag, position, progress, time allowance or winner is assigned by the diagnostic.

The player checks actual tuning-file balances, independently reloaded battle history, duplicate native and managed result delivery, points/Continue, Yes/Yes, the same selected save and connection on rematch, and No returning to mode select. An unrelated car profile must remain byte-identical. The runner requires both processes to exit cleanly, cross-checks their race IDs and finish times, verifies all reports, and re-hashes the private and original distributable files. Ordinary save paths and public matchmaking are not used. The private player contains the supplied ROM and must never be uploaded.

The native `online_scored_finish` test separately checks that the guidance is read-only and can finish with either slot using the wheel ADC mapping. It covers a stationary opponent and two moving cars. In the moving case, the trailing driver waits three seconds after GO and must still be racing beyond 90% of the course when the leader finishes. This verifies immediate winner selection without waiting for the opponent to finish or time out. Passing that numerical test does not establish the packaged reward flow. Passing the packaged test would still leave a connected race with both cars moving, process-restart recovery, disk failures, power-loss atomicity, EOS and different-network acceptance unproven. Captures from these hidden players also require separate visual review.

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

## Unity downloads and previous-release upgrades

Build the isolated diagnostic scene and the fresh candidate sequentially, without editing gameplay code between them. Do not run both Unity builds simultaneously: the diagnostic builder temporarily creates a scene inside the project.

```powershell
python -X utf8 Tools/build_updater_diagnostic.py --output Verification/updater-diagnostic
python -X utf8 Tools/build_release_candidate.py --original-image D:/reference/idas3_main_0C020000.bin --output Verification/fresh-candidate
python -X utf8 Tools/verify_updater_flow.py --diagnostic-player Verification/updater-diagnostic/player --build-evidence Verification/fresh-candidate --child <UpdateTestChild.exe> --output Verification/updater-flows
```

Use the `UpdateTestChild.exe` produced by the source verification suite's updater build. The flow runner checks that the diagnostic's managed assembly is byte-identical to the fresh candidate. It then exercises the real Unity downloader, staging, shutdown and embedded native helper against disposable loopback fixtures. Its nine cases cover full/patch installation, damaged-retained-file fallback, rejected core/patch downloads, separate course installation, fallback to both full downloads, a corrupt course download and an interrupted course download. Successful fixtures restart a small test executable; they do not claim full-game startup.

The source suite separately stages both archives through production code and invokes both the compiled and bundled helpers. Twelve cases per helper cover installation, missing old courses, rejected archives, collisions and rollback after locking a course file. These are bounded regression fixtures, not the full upgrade journey.

To exercise the complete previous-release file set:

```powershell
python -X utf8 Tools/verify_release_upgrade.py --build-evidence Verification/fresh-candidate --diagnostic-player Verification/updater-diagnostic/player --baseline-archives <public-core.zip> <public-courses.zip> --output Verification/full-upgrade
```

The command reads the latest public GitHub release and requires both supplied baseline ZIPs to match its published names, sizes and SHA-256 digests. It packages the candidate into core/course downloads, or accepts `--target-archives <core.zip> <courses.zip>` after validating their combined inventory against the candidate. The development candidate must have a newer version than the baseline. Bumping that version does not publish a release or update the public installer URLs.

The old files are extracted into a new marked private installation with inert save, ROM, music and replay sentinels. The current diagnostic then downloads both full-sized archives from loopback, stages and applies them, and the native helper restarts the actual upgraded executable. The runner stops only that owned restart and invokes the upgraded game's blocked-ROM startup diagnostic. It checks all target file hashes, unchanged sentinels, safe download cleanup and unchanged candidate bytes. Unlisted legacy files are reported and retained. No actual ROM is required or distributed.

**Scope limitation:** this runs the current updater against exact old installation files, with the installed version explicitly simulated and recorded separately from the executing application's version. It does not execute the old shipped managed updater, exercise GitHub download reliability, test the full-size patch route, or prove normal gameplay after an upgrade. Those remain separate acceptance evidence. The result always leaves `releaseReady` false.


## Windows paths beyond MAX_PATH

The source suite exercises production staging, native installation, retained patch files, rollback, recovery and managed cache cleanup with actual filenames longer than 300 characters. It runs these fixtures against both the compiled helper and the embedded resource. The managed Framework harness enables modern path parsing in its own application configuration; it does not change Windows registry policy. To additionally exercise Unity's Mono runtime:

```powershell
python -X utf8 Tests/Updates/test_long_paths.py --helper <tested-helper.exe> --mono "C:/Program Files/Unity/Hub/Editor/6000.6.0f1/Editor/Data/MonoBleedingEdge/bin/mono.exe" --report-dir Verification/updater-long-paths-mono
```

The checks also reject damaged retained files, invalid recovery backups and junctions, preserve private files and their timestamps, and retain locked or uncertain cache data. They use inert files, so a full-sized Unity download/upgrade is still required after changes to the updater. DOS 8.3 alias tests in `test_native_paths.py` are separate: its historical `long-path` case means a non-8.3 spelling, not a path exceeding MAX_PATH. That suite needs a fixture volume with 8.3 names enabled; a missing short alias is a failed prerequisite, not evidence of an updater regression.

## Result-save recovery under development

The online result path now writes a checksummed intent before awarding native tuning points or committing managed battle history. Native preparation captures the actual disk before-image separately from the selected garage profile in memory. Applying the absolute after-image is conditional: it succeeds once, recognizes an identical previous application, or refuses a conflicting newer profile. Ordinary and conditional profile writers share an OS-held lock. Result history retains its race receipt, including when an administrative reset supersedes a pending recovered result.

The result journal has one outstanding result per save root, increasing generations, two current copies and a pending publication file. Both current copies must retain the completed generation before another result starts. Recovery runs before native scene initialization so offline gameplay cannot first load or modify an unrecovered profile. An unreadable pending intent or conflicting profile is preserved and blocks recovery. This is recoverable ordering across two files, not one atomic filesystem transaction or a verified power-loss guarantee.

`Tests/Multiplayer/test_result_journal.py` is part of the source verifier. Its default run simulates the native profile boundary while exercising real journal/record files and terminating only owned test children. To additionally exercise the actual native recovery ABI without initializing a scene or using player saves:

```powershell
python -X utf8 Tests/Multiplayer/test_result_journal.py --native <fresh-Idas3Unity.dll> --report-dir Verification/result-journal-native
```

The process interruption cases cover preparation, durable intent, tuning publication, history publication, completion publication, and completed results. Separate native tests cover per-car writer exclusion, crash-released locks, profile conflicts, exact field preservation, backup repair, secondary garage cars and the tuning cap. The session suite checks failed native saves and result-screen retries as well as history failures, departure and resets.

The new packaged candidate must still prove startup recovery through `Idas3SceneGame`, normal scored races/rematches with the journal enabled, and the retry/exit panel with keyboard, controller and supported languages/resolutions. The source tests and direct ABI driver do not establish those player-facing outcomes.

### Packaged recovery acceptance

The maintained startup check uses a passed native journal-driver report and an unchanged fresh Windows candidate:

```powershell
python -X utf8 Tools/verify_result_startup.py --build-evidence Verification/fresh-candidate --helper-evidence Verification/result-journal-native --rom "D:/original-game/gds-0033.chd" --output Verification/result-startup-player
```

It terminates only its owned helper after durable intent, tuning publication, history publication, completion publication and full completion. Each private fixture enters the actual `Idas3SceneGame` startup path. Recovery must finish before native scene readiness, retain the opaque profile fields, award exactly one result, and leave both journal copies complete. A second actual startup must preserve the exact reward/history/journal bytes. These players are deliberately stopped after startup observation; this is not a clean-exit, natural-race interruption, EOS or complete multiplayer-smoke pass. The original distributable inventory is rechecked without treating the private marker or supplied ROM as release content. Never upload the private player.

To exercise the save-error modal during naturally completed scored races:

```powershell
python -X utf8 Tools/verify_scored_player.py --build-evidence Verification/fresh-candidate --rom "D:/original-game/gds-0033.chd" --output Verification/scored-result-recovery --result-recovery
```

This optional mode obstructs only marked private battle-history writes: an empty directory prevents first publication, and a read handle prevents replacement of existing history on the rematch. It requires the native tuning award and durable intent while history remains unchanged; Continue must stay blocked. Generated held keyboard bits and a virtual controller use production menu routing. The check traverses Retry/Exit, exercises focus-loss and held-confirm guards, verifies Cancel cannot leave an unfinished result, then removes its own obstruction and activates Retry through the actual menu. Both peers use both input types over two races. Existing scored checks still verify rewards, duplicates, connected rematch and No returning to mode select.

Actual modal `OnGUI` Repaints are captured for English, Japanese and Chinese at 1024x768, 1280x720, 1920x1080 and 2560x1080. Their surrounding area is deliberately black; they establish the modal's rendering, not the native background. The runner validates all capture dimensions and hashes, but separate visual inspection is required. Physical controllers, clicking Exit, loss of disk space before durable intent, natural race-process termination and power loss remain outside this diagnostic. A passing source suite or editor compile does not establish that this optional packaged mode passed.
