# Unity-owned original UI

The Unity scene conversion draws the existing playable game's UI using Unity meshes, materials, textures and command buffers. It does not upload the finished native HUD/menu framebuffer. Native original owner/controller state still determines the authored sprites, glyphs, transforms, colors, timing and draw order.

## Implementation

- `src/unity_ui_capture.h/.cpp`: opt-in command recorder and texture transfer ABI. `UnityUiVertex` is24bytes; `UnityUiDraw`48bytes; `UnityUiFrame`40bytes. Draws retain source TSP/PCW, pixel-space triangle vertices, diffuse/offset colors, opacity and clip rectangle.
- `src/native_assets.cpp`: shared menu/sprite triangles record commands and skip their per-pixel raster loop only when capture is enabled. Existing software raster output remains available with capture disabled.
- `src/frontend.cpp`, `src/ui.cpp`, and the mode/gasstand/demo/ranking/tuning painters: explicit logical-surface clears/copies, cached canvas transforms, independent fades, map lines and GDI host-label glyphs. Native original Japanese/name text remains authored sprite/font-bank geometry. Only desktop debug/control labels use individually rasterized Windows font glyph textures.
- `Assets/Scripts/Idas3UnityUi.cs`: persistent source Texture2D assets, batched adjacent triangles, source material blend factors, background camera atdepth−1, foreground camera atdepth2, final owner fade after the rear-view camera. C# calls `SetInteger` for raw shader words.
- `Assets/Resources/Idas3Ui.shader`: point sampling, original clamp/mirror/wrap, texture environments, offset color and original blend states. Shader lives in Resources for inclusion in builds.

Capture is synchronous and single-threaded with `Idas3SceneStep`; accessors borrow current frame state. Enable before App initialization, BeginFrame before its render, and submit each original overlay once in source order. BeginFrame clears the output list while retaining surface command caches. Surface clears replace captured commands even when a vector reuses the same allocation. Renderer submission resolves captured surfaces; missing surfaces increment `unresolvedSurfaces`. The Unity component reports this count as an error and never substitutes a framebuffer texture. Root's Main integration supplies allocation/copy hooks for its Name and tuning background/layers and ranking overlay copy.

Normal imported UI texture banks remain immutable for App lifetime. Transient640×480 ranking/tuning canvases are copied as command lists rather than registered as images. Host font glyph textures are owned by the recorder. The pointer/size identity cache for immutable source images is reset with App shutdown/reinitialization; a future in-session mutable image/bank reload would need a content revision or explicit invalidation API.

## Verification

`BridgeBuild/test_ui_capture.cmd` passed97 checks. It verifies texture-byte transfer, UV-to-corner binding, clip transforms, surface persistence, clear/reuse, primitive capture, background/additive flags, unresolved-surface reporting, all three original Mode choices through five confirmation phases, absence of software fragment-cache allocation, and exact restored software output when capture is disabled.

`BridgeBuild/test_ui_screens.cmd Verification\NEW-unique-folder` compiles against the current Unity native objects and libraries. The latest run is `Verification/unity-ui-screens-002`:1003 checks, all12Frontend stages at640×480 and1280×720, all35cars and color changes, imported Name, actual separate Name/tuning-course layers, eight naturally progressed attract owners,18ranking boards, three result-tuning kinds/descriptions and six TA/Legend/Bunta result states. Captured cached/paused commands match exactly and all surfaces resolve. The fixture writes `screen-commands.csv`; it creates no graphics device or driver-save directory. Ranking's unused3D assembly method is an explicit throwing boundary in this UI-only fixture.

The Unity player/C# shader build completed successfully on2026-09-06 (`Logs/BuildUnityScene.log`). Command coverage and successful shader compilation do not establish framebuffer equivalence. Actual Unity screenshot review is coordinated by the parent integration task. Native source controllers and handling are not changed by this UI port.
