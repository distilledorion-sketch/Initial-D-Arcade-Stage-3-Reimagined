# Unity bridge contract

The Unity plugin hosts the copied0.3.29 `App` in-process. `unity_bridge.cpp` includes `main.cpp` with `IDAS3_UNITY_PLUGIN`, which excludes WinMain. It creates no native window or second game process. The copied handling, physics data, frontend, original race owners, rendering preparation, attract sequence, tuning/results and audio mixer remain the existing implementation. This is a Unity host around that implementation, not a rewrite using Unity physics.

## ABI and thread ownership

`unity_bridge.h` is the authoritative ABI. Version1 uses cdecl exported functions, a stdcall render callback, Pack8 structures, input88bytes and status80bytes (native static assertions).

1. Queue initialization with explicit UTF8 asset/save roots, a live Unity D3D11 Texture2D pointer, desired native target dimensions, and whether to open native audio output. Queueing retains a COM reference to the input texture.
2. Issue each returned positive token once, in order, using `GL.IssuePluginEvent`. Initialization derives the D3D11 device from that texture on the render callback. The renderer uses a private deferred context and restores Unity immediate-context state when submitting.
3. Wait until status.state==1 before submitting frames. Queue one copied input packet per Unity frame. The bridge derives original keyboard/controller edges from held input, then calls the existing commands, fixed60Hz simulation, render/owner updates, and audio service in the standalone order. It does not poll operating-system input.
4. Create or refresh a Unity external texture from the borrowed output pointer when textureGeneration changes. Output is BGRA8_UNORM. Root's C# host uses a fixed native resolution and scales its display.
5. Destroy managed external-texture wrappers, queue/issue Shutdown, and wait for that event to complete before unloading or restarting. `Idas3UnityWaitForEvent` only waits; it never runs graphics or application code on the calling thread. Keep the plugin loaded if a bounded wait times out.

All App lifetime, initialization, ticking, renderer resizing and shutdown run in the render callback. Repeated event tokens do nothing. API/callback exceptions are contained, exposed by status/error accessors, and stop the failed App/audio without saving partially initialized state. The callback does not invoke MessageBox or legacy WinMain diagnostics. Normal shutdown saves settings and flushes pending profiles, then destroys original EngineAudio (including waveOut buffers) and graphics resources. The tiny process-lifetime runtime holder deliberately has no CRT/DllMain cleanup; the managed host must drain Shutdown.

Unfocused menus/attract stop advancing. Losing focus during a race pauses it and resets the fixed-step accumulator, matching the native window policy; regaining focus leaves race pause active until the existing pause control is pressed. The existing FixedClock still limits catch-up to six60Hz steps and0.1seconds, preserving its handling order. Unity output is rendered without another physics loop.

Resizing retains previously exported native texture references until shutdown so a Unity main/render-thread generation handoff cannot expose a freed texture. The current bridge permits16 target allocations per session, then reports an error; the provided C# host deliberately keeps a fixed target. Dynamic unlimited resizing would need an explicit external-texture retirement handshake.

## Save isolation

The only copied Main changes are a `saveRoot`/`userdataRoot()` accessor, routing existing save/read sites through it, and the WinMain guard. Existing standalone defaults remain root/userdata. Unity must provide a separate directory; root/data and root/userdata are rejected. Settings, selected car, driver profiles/setup markers, TimeAttack records and ghost/telemetry paths all use that directory. No schema or original profile logic changes.

The managed host chooses `Application.persistentDataPath/userdata`. Copied Native/userdata is evidence and is not used as live Unity storage unless explicitly and unsafely bypassing the exported configuration guard.

## Bounded checks

`tests/unity_bridge_smoke.cpp` dynamically loads the DLL and invokes the exported callback using a WARP-created stand-in Unity texture. It uses a **new isolated output directory**, opens no window/audio device, and checks actual App initialization/attract, F5 TimeAttack launch, original acceleration/60Hz ticks, focus pause/refocus, repeated token suppression, identical paused pixels, shared-device resize, isolated settings, shutdown and restart. It captures initial, driving and paused BMPs. It does not execute the Unity Editor or test real audio-device output.

Standalone compile script from workspace root: `outputs/InitialDUnity/BridgeBuild/compile_smoke.cmd`. Invocation:

```text
unity_bridge_smoke.exe <Idas3Unity.dll> <asset root containing data> <NEW isolated output directory>
```

The agent's preliminary check passed905 assertions under `BridgeBuild/smoke-001`, using workspace assets read-only. Root's final registered run against the clean copied Native assets also passed905 checks. Final DLL SHA256 reported by root: `CE61F1E202F2B4C5A2F121BC6A2B3F005F011FA06DFE6F1920F996FD685B2E42`. The C# host now gates frame submissions until native initialization reports ready. Unity Editor/runtime execution itself remains unverified because no Unity installation is available.

**Final clean-copy provenance:** `BridgeBuild/clean-copy-provenance.json` independently compares54 physics-data/core handling/input/start files and all26 copied save files against the actual clean0.3.29 origin, `C:\Users\Developer\Documents\Claude_Handoffs\IDAS3_Native_Remake_Handoff_2026-09-06\playable-project`. All80 SHA256 hashes and all80 `LastWriteTimeUtc.Ticks` values match exactly: zero mismatches. This comparison was made after the native bridge tests completed. The preliminary workspace comparison used an older save history and is superseded by this clean-origin audit.
