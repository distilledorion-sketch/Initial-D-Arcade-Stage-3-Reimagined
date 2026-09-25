# Recovered meter import

`Import-ArcadeMeters.py` imports the 87 entries selected by the recovered The Arcade S3 meter registry. The default input is `D:/Initial D games/Extracted HUD Assets - The Arcade S3`. Override it with `--source` when the extraction lives elsewhere. The input must contain `Catalog/Meter Audit` and `Exports`.

```powershell
python Tools/Import-ArcadeMeters.py
# Rebuild metadata while checking the already imported texture copies:
python Tools/Import-ArcadeMeters.py --no-copy
```

The importer copies original converted PNG/HDR bytes without recoloring or resampling them, verifies their SHA-256 hashes, and emits the widget layout, material parameters, animation curves and alpha bounds. `ArcadeMeterNames.json` contains the display names. Missing source IDs 16, 33 and 59 are not invented. Saved style 0 remains Original and 1 remains Stuttgart; other styles use source ID + 2, so list changes do not change saved selections.

## Runtime adaptation

The Unity renderer supplies speed, RPM, gear, AT/MT, pedals, night state and the native drift signal to the recovered art and animation channels. It does not execute the original widget bytecode. Prius intentionally has no tachometer or gear readout. Individual designs expose different indicators.

Source masks are sampled in their retainer canvas, separately from animated children. Circle01 and Circle02 have opposite sweep conventions, established from the recovered polar-coordinate texture, artwork and curve endpoints. The unused duplicate Stuttgart brake animation is excluded. The Retrowave transmission textures use the opposite 01/02 convention to the other families. DAC White uses dark speed text for contrast. Sirius uses a normalized RPM reveal for its black cover because the original negative shader offsets cannot be directly reused.

An image's specified brush tint is retained separately from its animated widget color and multiplied after animation evaluation, matching Slate's image composition. This restores Steampunk's amber nixie and coil glows and their opacity, rather than drawing them as white light. Steampunk's native drift/rev activation also preserves the authored layer opacity; a source overlay with nonpositive opacity is not enabled by the warning gate. `Verify-MeterBrushTint.py` checks all 103 specified tints against the source. Foreground-style references without a recoverable local color remain unchanged and are recorded as unresolved style dependencies. The original texture bytes and reconstructed glow formula are unchanged.

Omitted OverlaySlot alignment uses the native Left/Top defaults, preserving the Miku families' authored frame and audio-glow sizes. Explicit Fill and centered-padding behavior are unchanged. Miku silhouette needles use the Meter00 day/night textures registered by their cooked constructors; the raw Meter49 template brush remains recorded as source provenance. `Verify-MeterSourceLayout.py` checks the source anchors and compares geometry across all 87 meters.

When a meter supplies only daytime RPM faces, night races retain the matching RPM scale instead of reverting to its initial 8,000-RPM face. Existing day/night alternatives and AT/MT selection remain preferred. Halloween's recovered lantern and its glow subtree also play the original InOut rotation keys on native drift entry and exit, around the source hanging pivot. This elapsed-time adapter plays forwards on entry and backwards on exit, with no artificial startup event. The separate graded Blink state remains unavailable. Its swing envelope is included in the stable HUD bounds.

Cooked constructor registrations select the RPM faces, needle variants and night-only illumination widgets. Shared AB faces and Classic's separate A1/A2 layers retain their own frame families. Classic's warning animates the dial tint without hiding its base artwork; deactivation restores the neutral frame. Phoenix's gear flash uses the current gear, while Metallic's blur follows its recovered animated atlas index, wrapping across the seven-cell boundary.

The shared material adapters also compose Single-family secondary lighting and effect color, Racing pedal masks with their rotated gradient, and Retrowave's scrolling grid with its recovered tiling and edge-fade parameters. These material operators are reconstructed from the retained inputs, not recovered executable shader graphs.

Gear-change animations now follow actual telemetry transitions. The first observed gear does not trigger a flash; style changes, invalid telemetry and clock rewinds clear the event. Recovered key values animate the correct current-gear digit, glow, scale, rotation and Steampunk's rolling digit and chain scroll. `MovieScene.PlaybackRange` controls event duration. Missing tick resolution uses 24,000 ticks/second as an explicit adaptation, consistent with the documented [Unreal Engine 4 Sequencer default](https://dev.epicgames.com/documentation/unreal-engine/sequencer-time-refactor-technical-notes?application_version=4.27); the audit does not establish the original widget's effective tick resolution. The importer records whether a resolution was recovered.

Steampunk and Maou thunder use their full recovered flipbook atlases. Touhou silhouette decorations use the retained blink amplitude, offset and rate. Miku, Rin/Len, Luka, Shouni Penguin and Retrowave frames include both recovered textures with their material colors and rotation rates; fixed material texture samples are imported alongside texture parameters. The CPU passes animation state to the same shader path for the preview and in-race HUD. HUD bounds include transient gear animations and moving-UV quads.

Future, Future Aqua, Future Purple, Future Zero and Future 2023 have ten authored trail images for each needle. The recovered `Apply_CenterPinTrail` / `Apply_LeftPinTrail` functions read RPM and speed angle histories: trail 01 uses the current angle and trail 10 uses history index 9. These images now follow the owning needle's rotation curve instead of retaining their serialized default angles. The adapter resamples observed needle angles at a fixed 60 Hz so trail duration is independent of display FPS; this cadence is an explicit reconstruction because the cooked data does not retain a rate. Startup, scale changes, lost telemetry, seeks and gaps longer than the trail lifespan seed the history at the current needle. Frozen clocks preserve the trail, and same-time telemetry replacements reset it without manufacturing motion. Original positions, pivots, artwork and opacity are preserved.

Audio visualizers on sources 68, 69, 70 and 74 now use 32 frequency bands from actual mixed game output, including custom music. The DSP callback copies PCM into a preallocated ring; the main thread runs the analyzer. There is no microphone input or RPM-driven substitute. Stereo energy cannot cancel between opposite-phase channels. Band smoothing uses sample time, not display frames. Each renderer owns its spectrum texture; the presentation clock holds its last picture during a pause, with silence, mute and shutdown clearing stale bars.

Maou's aura combines both recovered flame masks with the original moving noise textures and material colors. Steampunk's procedural ball glows use the recovered radius/density/diamond parameters and native drift, rev and pedal activation. Shouni Penguin's LED display now cycles the penguin, CHUNITHM, NEW and SEGA programs, using their authored scrolling curves and playback lengths with the recovered dot-grid and aperture masks. The six-row penguin atlas uses one cycle per second. Gear-shift frame-speed curves are integrated continuously so shifts accelerate the rotation without jumping or resetting its angle.

Several material operators and event graphs were stripped from the extraction. Flipbook rates interpreted as cycles/second, sinusoidal blinking, two-layer translucent frame composition, radial glows, aura compositing, radial spectrum bars and LED program ordering are functional reconstructions, not a claim of exact arcade shader parity. The LED playlist uses serialized switcher order because the original event chain is unavailable. The Retrowave line's original color gradient is approximated. Original-only corner-speed and colored drift-grade inputs remain unavailable; speed-event flashes remain disabled because their activation rule is unavailable. Disabled source layers and reasons remain in `catalog.json` and the import manifest.

## Verification and build

Run Unity with a graphics device (omit `-nographics`):

```text
-batchmode -force-d3d11 -quit -projectPath <repo> -executeMethod Idas3MeterCatalogBuild.VerifyOnly -logFile <log>
```

This checks the catalog/settings and renders every meter through the production GPU renderer. It also bakes stable content bounds with catalog/alpha hashes, so browsing meters does not sample hundreds of animation states in the player. Missing or stale bounds fall back to sampling. Bump `RendererVersion` in `Idas3MeterLayoutBounds` when changing composition or bounds sampling, then rerun verification. `VerifyAndBuild` performs the checks and builds the local Windows player. The render report and actual low/high PNGs are written to a fresh directory under `Verification/meter-import-20260924`.

For real menu, persistence, layout and race checks, run the player with:

```text
-idas3-attract-options-smoke <new-output-directory> -idas3-hud-customization-check
```

The smoke run uses private saves. It still requires the user's original disc locally; no ROM is supplied by the import or build.
