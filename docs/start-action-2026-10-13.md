# Configurable Start action: implementation and verification record

Date: 2026-10-13 (Asia/Taipei)

## Scope implemented

- `Idas3ControlBindings.cs` appends Start at action index 16, defaults it to unbound, restricts it to digital controller controls and existing permitted keyboard keys, migrates controls v1–v4 to v5, resolves held Start from the active profile and rig, preserves the original Pause defaults, and limits direct gear iteration to Gear 1–6.
- `Idas3ControlBindings.Rig.cs` identifies Start contributed by an already eligible saved generic rig device. Existing rig admission rules remain in place.
- `Idas3ControllerDevices.cs` and `Idas3ControllerControl.cs` associate a sampled button with the raw pad bits that same source emits, including generic first/second button and Joystick trigger fallbacks.
- `Idas3SceneGame.cs` routes bound Start through the existing menu/race frame path, blocks same-source aliases before managed menu input, and retains focus, modal, and online filtering. `Idas3PauseMenu.cs` exposes the new row through its existing scrolling, capture, clear, draft, and Apply flow.
- The native ABI and gameplay code are unchanged. Start is submitted as a connected pad Start bit to native menu and result owners. The existing offline race pause route still uses its managed Escape translation, while physical Escape remains independent.

## Tests added or updated

- `Idas3StartBindingChecks.Run`: explicit 9/10/16-action historical fixtures for v1–v4, all profiles, save/reload, invalid analog Start, reserved keyboard keys, conflict swap, save failure, direct gear isolation, generic A fallback, release guards, second rig source, and menu capture/Apply/Clear.
- `Idas3ControlsSmoke` with `-idas3-start-check`: production Input System discovery, UI capture of controller A as Start, UI Apply, fresh bindings reload, native offline pause status and held/release/repress behavior, and UI Clear.
- `Idas3ControllerDevicesSmoke` with `-idas3-start-check`: production Joystick discovery and trigger-button capture, raw-A alias suppression, offline pause status, held protection, and release/repress.
- `Native/tests/unity_bridge_smoke.cpp`: connected pad Start pause/hold/repress and disconnected pad bit characterization.
- Existing headlight and private shifter checks now expect v5/17 actions; `Idas3PrivateShifterChecks.Run` invokes the new binding checks.

## Environment and TDD status

This macOS checkout has no Unity editor, C# compiler, CMake, CTest, or Wine in PATH; `/Applications/Unity/Hub/Editor` is absent. The Windows native bridge test requires `windows.h`, D3D11, a built DLL, and WARP. Related baseline tests, new Red assertions, Green reruns, and regressions were **not executable here**. Tests were written before their corresponding implementation changes, but no Red/Green execution is claimed. No original test failures were observed because the baseline could not run.

Executed: `git diff --check` (passed). This is formatting validation only.

## Reproduction commands on a Windows development host

Use a Windows host with the project's Unity editor and native build prerequisites. Replace `<project>`, `<Unity.exe>`, `<InitialDUnity.exe>`, `<built-Idas3Unity.dll>`, and `<new-output>` with local paths. The output directory for player and bridge smoke must not already exist.

```powershell
& <Unity.exe> -batchmode -quit -projectPath <project> -executeMethod Idas3StartBindingChecks.Run -logFile <new-output>\start-bindings-editor.log
& <Unity.exe> -batchmode -quit -projectPath <project> -executeMethod Idas3PrivateShifterChecks.Run -logFile <new-output>\shifter-regression.log
& <InitialDUnity.exe> -idas3-controls-smoke <new-output>\start-player -idas3-start-check
& <InitialDUnity.exe> -idas3-controller-devices-smoke <new-output>\start-devices -idas3-start-check
cmake -S <project>\Native -B <project>\Native\build -G "Visual Studio 17 2022" -A x64
cmake --build <project>\Native\build --config Release --target Idas3Unity unity_bridge_smoke controller_response_tests
ctest --test-dir <project>\Native\build -C Release --output-on-failure -R controller_response
& <project>\Native\build\Release\unity_bridge_smoke.exe <built-Idas3Unity.dll> <project>\Native <new-output>\bridge
```

Expected reports: Unity editor logs above, `<new-output>\start-player\report.json`, `<new-output>\start-devices\report.json`, CTest output, and `<new-output>\bridge\bridge-smoke.txt`. Additional existing Unity smoke entrypoints named in the task must be run before software acceptance.

## Acceptance status

| AC | Corresponding tests | Status |
| --- | --- | --- |
| 01 defaults and old behavior | `Idas3StartBindingChecks`, existing `Idas3ControlsSmoke`, `Idas3OnlineControllerSmoke` | Unverified |
| 02 migration | `Idas3StartBindingChecks`, `Idas3HeadlightBindingChecks`, `Idas3PrivateShifterChecks` | Unverified |
| 03 setup and persistence | `Idas3StartBindingChecks`, `Idas3ControlsSmoke -idas3-start-check` | Unverified |
| 04 frontend | Existing `Idas3AttractOptionsSmoke` and new routing path; bound-Start frontend state comparison still needs an automated assertion | Unverified |
| 05 race and flow | `Idas3ControlsSmoke -idas3-start-check`, `unity_bridge_smoke`; bound-Start result and held-skip state comparisons still need automated assertions | Unverified |
| 06 same-source aliases | `Idas3StartBindingChecks`, `Idas3ControllerDevicesSmoke -idas3-start-check`; second generic button and descriptor hat in full player still need a bound-Start assertion | Unverified |
| 07 independence | `Idas3StartBindingChecks`, existing `Idas3WheelRigChecks` and `Idas3PrivateShifterChecks` | Unverified |
| 08 held/release | `Idas3StartBindingChecks`, `Idas3ControlsSmoke -idas3-start-check`; all owner transitions need player assertions | Unverified |
| 09 input protection | Existing online/capture/focus smoke; new bound-Start online race assertions still needed | Unverified |
| 10 UI | `Idas3StartBindingChecks`, `Idas3ControlsSmoke -idas3-start-check`; visual UI inspection pending | Unverified |

Software acceptance is **not complete**. Physical wheel/HID capture, save/reload, use, and reconnect are **not verified** because no device or Windows player is available in this environment.
