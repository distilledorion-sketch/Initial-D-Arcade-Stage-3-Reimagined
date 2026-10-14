# Configurable Start: Windows build and acceptance evidence

This is the Windows follow-up to [the 2026-10-13 handoff](start-action-2026-10-13.md). All times below are Asia/Taipei. Full logs and isolated diagnostic output are saved under
`C:\Users\User\.codex\visualizations\2026\10\13\01a1354c-d23b-75c2-9fcd-10d2153e74ab` (called `EVIDENCE` below). This file records observed results; it does not claim the unrun player assertions passed.

## Prerequisites and isolation

- The main repository was clean at `60d2053fbc4496b309aad3e4fcb12466586a7dc1`. No applicable `AGENTS.md` was found. `BUILDING.md` was read. C: had 190,405,230,592 bytes free before building.
- `idas3-start-handoff.zip` SHA-256: `269cc3fece6fa8473a2d2bbcb01347114421202f20088783d62de1b9e4a11542`.
- The ZIP was extracted outside the repository to `EVIDENCE\handoff`; `start-action.patch` SHA-256: `f3ac2bd5f1fac54ea6215eb8964d22e21c9396f064159f221cbfa63e326dbb53`. Its `MANIFEST.json` base commit matched. The handoff's example checkout path was superseded by this task's repository path.
- A detached clean-HEAD baseline worktree was placed at `EVIDENCE\baseline-writable`. The first Codex-managed worktree was read-only to Unity; the separate writable worktree was used. Its long absolute path caused baseline asset packing to fail when appending `.packing` to a 254-character file path. A `subst B: EVIDENCE\baseline-writable` mapping allowed the same baseline player to build without changing source.

## Commands and results

| Stage | Command or entrypoint | Result and evidence |
| --- | --- | --- |
| Baseline native script | `cmd /c "Build Native.cmd"` in baseline | Exit 1: `vcvarsall.bat` changed the current directory, so CMake sought `Native` under Visual Studio. `EVIDENCE\baseline-native-build.log`. |
| Baseline native workaround | `vcvarsall.bat x64`, `cd /d <baseline>`, `cmake -S Native -B Native/build-unity-d -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release`, `cmake --build Native/build-unity-d --target Idas3Unity controller_response_tests unity_bridge_smoke` | Exit 0. `EVIDENCE\baseline-native-elevated.log`. An initial sandbox compiler probe failed with C1902; the normal Windows invocation succeeded. |
| Baseline Editor | `Unity.exe -batchmode -quit -projectPath <baseline> -executeMethod <check> -logFile <log>` | Private Shifter, Headlight Binding, Controller Menu, Wheel Rig all exit 0. Reconnect initially exited 1 because its required `-idas3-reconnect-check-output <new-dir>` argument was omitted; corrected run exited 0 with `passed=true`, `checks=380`, `failures=[]`. The qualified `Idas3.Multiplayer.Idas3OnlineMenuChecks.Run` also exited 0. Individual `baseline-*.log` files and `baseline-reconnect-report\report.json`. |
| Baseline player | `Unity.exe -batchmode -quit -projectPath B:\ -buildTarget Win64 -executeMethod Idas3Build.BuildWindows -logFile EVIDENCE\baseline-player-shortpath.log` | Exit 0 after the long-path attempt exited 1. Complete runtime staging confirmed in baseline `Logs\staging.log`. |
| Baseline Native CTest | `ctest --test-dir <baseline>\Native\build-unity-d --output-on-failure -R '^(controller_response|frontend|attract_integration|story_progression|online_race_timeline|original_host_input)$'` | Exit 0; 6/6. `EVIDENCE\baseline-related-ctest.log`. |
| Patch | `git apply --check EVIDENCE\handoff\start-action.patch`, then `git apply EVIDENCE\handoff\start-action.patch`, then `git diff --check` | All exit 0. New Start check, `.meta`, and handoff document appeared. |
| Patched Native | `cmd /c "Build Native.cmd"` | Exit 0 after restoring `cd /d "%~dp0"` following `vcvarsall.bat`. `EVIDENCE\patched-build-native.log`. |
| Patched Unity | `cmd /c "Build Unity.cmd"` | First run exited 1 on two `int` to `uint` Start-mask compile errors in `Idas3ControlBindings.cs` (`Logs\Build.log`); two explicit casts fixed them. The observed compiler lines were transcribed to `EVIDENCE\patched-build-unity-red-observed.txt` because the later Unity run overwrote `Logs\Build.log`. Final full run exited 0; `EVIDENCE\patched-build-unity-green.log`, `Logs\Build.log`, `Logs\staging.log`. A final `Idas3Build.RebuildWindowsScripts` run exited 0 after adding the Editor HID checks: `EVIDENCE\patched-player-scripts-final.log`. |
| Patched Editor | `Unity.exe -batchmode -quit -projectPath <repo> -executeMethod <check> -logFile <log>` | Start Binding: 237 checks; Private Shifter: 59 plus nested Start 237, Wheel Rig 19, Controller Menu 181; Headlight Binding, Wheel Rig, Controller Reconnect (380 checks) all exit 0. Qualified Online Menu check exits 0. Logs: `EVIDENCE\patched-start-hid-editor-initial-green.log`, `patched-private-shifter-final.log`, and `patched-Idas3*Run.log`; Reconnect report: `patched-reconnect-report\report.json`. |
| Patched Native CTest | Same six-target CTest selection against `<repo>\Native\build-unity-d` | Exit 0; 6/6. `EVIDENCE\patched-related-ctest.log`. |
| Bridge smoke | `unity_bridge_smoke.exe <this-build-Idas3Unity.dll> <repo>\Native <new-dir>` | The patch initially failed to compile because `XINPUT_GAMEPAD_START` lacked `<Xinput.h>`; after that fix, the original 240-frame solver assertion failed before Start checks. Diagnostics showed `simulationTicks=0`, `flags=28` (running with original handling during countdown). Waiting up to 900 frames for 240 simulation ticks preserved the threshold; the corrected harness passed 1,669 assertions against both the baseline DLL and this-build DLL. `EVIDENCE\patched-native-tests-build.log`, `patched-native-tests-build-2.log`, `patched-bridge-diagnostic.log`, `patched-bridge-final.log`, `baseline-bridge-corrected.log`, and `patched-bridge-final-report\bridge-smoke.txt`. |

The shipped native DLL SHA-256 is `a04d1c41d8c9b807066e83a9d1fc9f347d84e236fa930642a86b4a0a8a2fe4f9`; the copy beside the built player matches. `Builds\Current\InitialDUnity.exe` SHA-256 is `a4c71b1bfc3e42d02f2f129fe04adef3ebf2343a662884e4b99598e9dfc90e7d`. The final scripts-only build updated `Builds\Current\InitialDUnity_Data\Managed\Assembly-CSharp.dll` on 2026-10-14 at 08:34:48; its SHA-256 is `82086d70f870219f12ddf054a42426527002e6c5be1fdc05bace2e6737c3b2a1`.

## Initial player smoke and ROM limit (historical; superseded below)

The following correct player entrypoints were attempted with separate fresh output directories: `-idas3-controls-smoke <dir> -idas3-start-check`, `-idas3-controller-devices-smoke <dir> -idas3-start-check`, `-idas3-pause-smoke <dir>`, `-idas3-attract-options-smoke <dir>`, `-idas3-mode-flow-smoke <dir>`, and `-idas3-online-controller-smoke <dir>`. Full commands and outcomes are in `EVIDENCE\patched-player-smoke-summary.json`; individual logs are `EVIDENCE\patched-smoke-*.log`.

Each log ends with `GDS-0033 startup check: blocked: Place gds-0033.chd in the rom folder.` The process was stopped after a 30-second bound. No `report.json` was produced for any run; `passed`, `shutdownComplete`, `error`, and assertion count are therefore **unknown**, not passes. A baseline controls smoke had the same ROM gate and was stopped after more than five minutes without a report. No ROM was read, copied, supplied, or bypassed. Physical wheel/HID testing was not performed. The Editor HID tests used a synthetic Unity descriptor, not physical hardware.

## Red, Green, and acceptance

- Red → Green: original `Build Native.cmd` directory failure → one-line directory restore and successful native build.
- Red → Green: Start C# `int`/`uint` compiler errors → two casts and successful Start Editor check and player build.
- Red → Green: bridge missing XInput declaration → header include and successful compilation.
- Red → Green: pre-existing bridge assertion made during race countdown → measured status and changed only the wait window, then 1,669 checks passed on both DLLs.
- Two Start Editor failures were caused by a `null` versus empty string comparison after Unity JSON deserialization. The fixture comparisons were normalized without changing binding behavior or removing assertions. The original 227 checks then passed.
- The new synthetic descriptor second-button and hat-direction assertions passed on their **first** execution; no Red result is claimed for that addition. The final Start check count is 237.

| AC | Status | Executed evidence and remaining limit |
| --- | --- | --- |
| AC-01 defaults and old behavior | 未驗證 | Start defaults, existing Editor checks and baseline/patched CTest pass; full player behavior is ROM-gated. |
| AC-02 migration | 通過 | Start Editor historical v1–v4 fixtures plus Headlight and Private Shifter checks pass. |
| AC-03 setup and persistence | 通過 | Start Editor capture, Apply, reload, Clear, reload pass; player path is ROM-gated. |
| AC-04 frontend | 未驗證 | Native `attract_integration` passes, but bound Start through the Unity host cannot run without ROM. |
| AC-05 race, result, held-skip | 未驗證 | Bridge offline pause/hold/repress and Native story progression pass; bound Start result and held-skip player assertions remain unrun. |
| AC-06 same-source aliases | 未驗證 | Editor synthetic descriptor second button and hat direction, generic source alias, and Start checks pass; full player HID route is ROM-gated. |
| AC-07 independence | 通過 | Wheel Rig, Private Shifter, direct-gear and Start Editor checks pass. |
| AC-08 held/release lifecycle | 未驗證 | Start Editor and bridge pause hold/repress pass; all player owner transitions remain unrun. |
| AC-09 input protection | 未驗證 | Existing online menu Editor and online race timeline CTest pass; bound Start during an online player session remains unrun. |
| AC-10 UI | 未驗證 | Editor controls-menu row, capture, Apply and Clear assertions pass; player visual inspection remains unrun. |

No test assertion was weakened to make incorrect Start behavior pass. Full software acceptance needs the original GDS-0033 dump installed through the normal `Builds\Current\rom` instructions, followed by rerunning the six player smokes and reading their reports. Physical wheel/HID acceptance requires an actual device and is separate from synthetic tests.

## ROM-enabled player follow-up

The user subsequently authorized use of the original `C:\dev\gds-0033.chd` strictly by copying it to the player runtime's `Builds\Current\rom\gds-0033.chd`. Before copying, the source was 236,940,011 bytes with SHA-256 `9be8db03c0f75415373937545786ac6f6b9d12627f0f0584ff9f2f5cd414cf8e`; the destination did not exist. After copying, both paths still have the same size and hash. The source remains in place. The runtime logs say `GDS-0033 startup check: verified CHD`. The ROM path is under Git's ignored `/Builds/` tree; no ROM bytes were placed in this evidence directory or source changes.

All the following commands use `Builds\Current\InitialDUnity.exe -idas3-<smoke> <EVIDENCE\fresh-dir> ... -logFile <EVIDENCE\log> -screen-fullscreen 0`. Output directories were new for every run. `Start-Process -PassThru` and `WaitForExit(300000)` recorded process exit codes. `report.json` was read separately for `passed`, `shutdownComplete`, `checks`, and `error`.

| Player invocation and output directory | Exit | Report | Log |
| --- | ---: | --- | --- |
| `-idas3-controls-smoke EVIDENCE\rom-controls-02 -idas3-start-check` | 0 | passed=true, shutdownComplete=true, checks=183, error="" | `rom-controls-02.log` |
| `-idas3-controller-devices-smoke EVIDENCE\rom-devices-05 -idas3-start-check` | 0 | passed=true, shutdownComplete=true, checks=134, error="" | `rom-devices-05.log` |
| `-idas3-pause-smoke EVIDENCE\rom-pause-visible-01` | 0 | passed=true, shutdownComplete=true, checks=280, error="" | `rom-pause-visible-01.log` |
| `-idas3-attract-options-smoke EVIDENCE\rom-attract-visible-01` | 0 | passed=true, shutdownComplete=true, checks=116, error="" | `rom-attract-visible-01.log` |
| `-idas3-attract-options-smoke EVIDENCE\rom-attract-start-02 -idas3-start-check` | 0 | passed=true, shutdownComplete=true, checks=122, error="", finalFrontendStage=1 | `rom-attract-start-02.log` |
| `-idas3-mode-flow-smoke EVIDENCE\rom-mode-01` | 0 | passed=true, shutdownComplete=true, checks=75, error="" | `rom-mode-01.log` |
| `-idas3-online-controller-smoke EVIDENCE\rom-online-01` | 0 | passed=true, shutdownComplete=true, checks=117, error="" | `rom-online-01.log` |
| `-idas3-mode-flow-smoke EVIDENCE\rom-finish-transition-01 -idas3-finish-transition-check` | 0 | passed=true, shutdownComplete=true, checks=6164, error="" | `rom-finish-transition-01.log` |
| `-idas3-mode-flow-smoke EVIDENCE\rom-mode-start-02 -idas3-start-check` | 0 | passed=true, shutdownComplete=true, checks=82, error="" | `rom-mode-start-02.log` |
| `-idas3-mode-flow-smoke EVIDENCE\rom-finish-start-02 -idas3-start-check -idas3-finish-transition-check` | 0 | passed=true, shutdownComplete=true, checks=6169, error="" | `rom-finish-start-02.log` |

Two isolated local LAN clients were also launched together with `-idas3-multiplayer-smoke <fresh-dir> -idas3-multiplayer-role host|join -idas3-multiplayer-port 29843 -idas3-multiplayer-pause-check -idas3-start-check -idas3-pause-no-capture` (the joiner also received `-idas3-multiplayer-address 127.0.0.1:29843`). The host and joiner `rom-online-race-*-red-01` runs each exited 0, with `report.json` recording passed=true, shutdownComplete=true, error="", and 17,492/17,431 checks. Their `pause-options-report.json` files record passed=true, online=true, error="", and 856/837 checks respectively. Each contains `online-bound-start-overlay-before` and `online-bound-start-overlay-after` observations; the checks confirm both peers' race clocks and snapshot counts advance while the bound Start options overlay remains open, without native pause. Both cases passed on their first execution, despite the diagnostic directory's preselected `red` name; the directory name is **not** a Red result. The logs are `rom-online-race-host-red-01.log` and `rom-online-race-join-red-01.log`.

The ModeFlow Start variants bind B through `Idas3ControlBindings` and preserve the mapped frame input through the existing private fixture adapter. They assert Start reaches native without B, and that a held press does not skip a second Bunta challenge/result owner or skip past post-FINISH analysis. They passed on first execution; no Red is claimed. The final Editor `Idas3StartBindingChecks.Run` exited 0 with 237 checks in `rom-final-start-editor.log`. The final player script build is `rom-scripts-result-red-07.log` (exit 0); `red` in that filename also records test intent, not a failing build.

The initial `rom-pause-01` and `rom-attract-01` runs used a hidden window and failed their OnGUI repaint assertions at checks 13 and 8, respectively. Their reports both recorded `shutdownComplete=true`. Rerunning the unchanged smoke assertions in visible windowed mode passed, as above. This was a capture/window visibility condition, not a changed expectation. `rom-devices-01` failed at check 5, automatic source selection, and `rom-devices-no-start-01` failed identically without the Start flag. The first timing change used 36 frames; `rom-devices-02` still failed at check 5. Diagnostic run `rom-devices-diagnostic-04` showed Beta West was pressed but only 0.322 seconds had elapsed since active Alpha input, within the intentional 0.5-second source lock. The fixture now waits 0.65 **real seconds** at rest before asking a different controller to take over. The same selection assertion then passed in `rom-devices-05`; no source selection behavior was changed. Intermediate build logs are `rom-scripts-01.log`, `rom-scripts-diagnostic-02.log`, `rom-scripts-diagnostic-03.log`, `rom-scripts-04.log`, and `rom-scripts-frontend-05.log` (all final respective script builds exited 0).

The added frontend Start assertion ran for the first time in `rom-attract-start-02` and passed. It binds Start to B through the current binding owner, verifies that the submitted packet contains Start without B or Escape, observes the actual Title advance, and holds B across the transition without repeated advancement. It is a new coverage case with **initial Green**, not a fabricated Red. The existing default Attract run also passed. The synthetic generic HID second-button and hat-direction binding assertions were added to `Idas3StartBindingChecks.Run` in the earlier build phase and passed on first execution. The `rom-devices-05` smoke further exercised Joystick trigger Start capture, raw A alias suppression, pause hold and release/repress through the player.

Visual inspection of actual captured images found the `rom-controls-02\controls-start-row.png` Start row visible and selected with controller A while Pause still shows Escape and controller Start. `rom-pause-visible-01\options-controls.png`, `rom-attract-start-02\attract-prompt.png`, and `rom-devices-05\devices-generic-bound.png` also rendered legibly. Images and report files are diagnostic evidence only. No physical wheel or HID device was attached for hands-on validation.

## Final AC status after ROM-enabled run

| AC | Status | Executed evidence and remaining boundary |
| --- | --- | --- |
| AC-01 defaults and old behavior | 通過 | Start Editor defaults, baseline and patched checks, controls/player Start smoke, and default Pause/Attract/Online smokes pass. |
| AC-02 migration | 通過 | Start Editor v1–v4 fixtures, Headlight Binding and Private Shifter checks pass. |
| AC-03 setup and persistence | 通過 | Controls player capture, Apply/reload/Clear/reload pass; Start row captured. |
| AC-04 frontend | 通過 | `rom-attract-start-02` binds B to Start and verifies original Title progression, alias stripping, Escape independence, and held transition. |
| AC-05 race, result, held-skip | 通過 | Controls and devices player pause/hold/repress; `rom-mode-start-02` and `rom-finish-start-02` bind B to Start and assert Bunta result/challenge held-skip and FINISH-to-analysis behavior. |
| AC-06 same-source aliases | 通過（合成裝置） | Start Editor descriptor second button and hat direction, player generic trigger/raw-A route pass. Physical HID remains unverified. |
| AC-07 independence | 通過 | Wheel Rig, Private Shifter, direct-gear and Start Editor checks pass. |
| AC-08 held/release lifecycle | 通過 | Title, offline pause, Bunta story/result, FINISH analysis, and online overlay held/release paths have player assertions; focus and capture checks also pass. |
| AC-09 input protection | 通過（本機 LAN） | Both actual local LAN clients pass bound Start overlay assertions: native race stays unpaused while both source clocks and network snapshots advance. Public online-service conditions were not exercised. |
| AC-10 UI | 通過（合成裝置） | Player Controls capture/Apply/Clear and inspected Start-row image pass. Physical wheel/HID UI use is unverified. |

These are software and synthetic-device acceptance results. No actual physical wheel/HID testing was performed. The bound result/story cases use the established native ModeFlow fixture with the production Unity binding mapper; the online race case uses an actual two-client local LAN session, not a public service.

## Final repository state and evidence index

`git rev-parse HEAD` remained `60d2053fbc4496b309aad3e4fcb12466586a7dc1`; `git diff --check` exited 0. The final `git status --short` has 16 modified tracked files (including the rebuilt native DLL, native script, native bridge test, binding/routing code and smoke fixtures) and four untracked handoff/evidence files: the Start checks `.cs` and `.meta`, `docs/start-action-2026-10-13.md`, and this document. The tracked `git diff --stat` is 398 insertions and 78 deletions across 16 files. Exact status and diff summary are preserved in `EVIDENCE\rom-final-git.txt`. Per-run report/log paths, passed/shutdown/check/error fields, and both ROM hashes are indexed in `EVIDENCE\rom-followup-summary.json`; the index contains no ROM bytes. No push, PR, deployment, or user-data cleanup was performed.
